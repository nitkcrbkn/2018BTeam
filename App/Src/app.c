#include "app.h"
#include "DD_Gene.h"
#include "DD_RCDefinition.h"
#include "SystemTaskManager.h"
#include <stdlib.h>
#include "message.h"
#include "MW_GPIO.h"
#include "MW_IWDG.h"
#include "MW_flash.h"
#include "constManager.h"
#include "trapezoid_ctrl.h"


static
int armSystem(void);//バネ引く関数

static
int bodyRotate(void);//射角決める関数


static
int put_bottle(void);//装填機構の関数

static
int throw(void);

static
int auto_con(void);//自動制御の関数

static
int LEDSystem(void);//LED周りの関数

static
int32_t I2C_Encoder(int encoder_num, EncoderOperation_t operation);



static int fire_motor = 0;//auto_conからbodyRotateへモータ回転伝える変数

//　射角変更
static int pull_motor = 0;//auto_conからarmSystemへモータの回転伝える変数

// 装填機構
static int load_bottle = 0;//auto_conからput_bottleへモータの回転伝える変数

static int throw_motor = 0;//auto_conからthrow関数へモータの回転伝える関数


static int start_position = -1;//スタート位置が赤コートか青コートか伝える


/*メモ
 *g_ab_h...ABのハンドラ
 *g_md_h...MDのハンドラ
 *
 *g_rc_data...RCのデータ
 */



#define WRITE_ADDR (const void*)(0x8000000+0x400*(128-1))/*128[KiB]*/
flashError_t checkFlashWrite(void){
  const char data[]="HelloWorld!!TestDatas!!!\n"
    "however you like this microcomputer, you don`t be kind to this computer.";
  return MW_flashWrite(data,WRITE_ADDR,sizeof(data));
}

int appInit(void){
  message("msg","hell");

  ad_init();

  message("msg","plz confirm\n%d\n",g_adjust.rightadjust.value);

  /*GPIO の設定などでMW,GPIOではHALを叩く*/
  return EXIT_SUCCESS;
}


/*application tasks*/
int appTask(void){
  int ret=0;

  /*if(__RC_ISPRESSED_R1(g_rc_data)&&__RC_ISPRESSED_R2(g_rc_data)&&
    __RC_ISPRESSED_L1(g_rc_data)&&__RC_ISPRESSED_L2(g_rc_data)){
    while(__RC_ISPRESSED_R1(g_rc_data)||__RC_ISPRESSED_R2(g_rc_data)||
    __RC_ISPRESSED_L1(g_rc_data)||__RC_ISPRESSED_L2(g_rc_data));
    ad_main();
    }*/
	 
  /*それぞれの機構ごとに処理をする*/
  /*途中必ず定数回で終了すること。*/
 	  
  ret = armSystem();
  if(ret){
    return ret;
  }

  
  ret = bodyRotate();
  if(ret){
    return ret;
  }
	 
  ret = put_bottle();
  if(ret){
    return ret;
  }
  ret = throw();
  if(ret){
    return ret;
  }
	  
  ret = auto_con();
  if(ret){
    return ret;
  }

  ret = LEDSystem();
  if(ret){
    return ret;
  }
  
  return EXIT_SUCCESS;
}


int auto_con(void){//自動制御

  
  ////////////エンコーダ関連/////////////////////////////////////////

  int32_t encoder_pull = I2C_Encoder(0,GET_ENCODER_VALUE);//DD_encoder1Get_int32();
  int32_t encoder_fire = DD_encoder1Get_int32();//I2C_Encoder(1,GET_ENCODER_VALUE);
  static int fire_angle = 0;//fire_angle_arrの値を代入する変数
  static int pull_spring = 0;//pull_spring_arreyの値を代入する変数

  
  /////////////フォトインタラプタ関係///////////////////////////////
  static int load_photo_count = 0;
  static int load_photo_support = 0;
  static int load_photo_looked = 0;
  static int load_count_go = 0;
  static int load_count_back = 0;
  static int load_count_stop = 0;

  

  ////////////状態視認変数//////////////////////////////////////////
  static int status = -1;//今何回目の射出か　０からスタート　−１で初期化
  static int default_angle = 0;
  static int position_look = ' ';

  ////////////再処理防止変数////////////////////////////////////////
  static int position_set = 0;//フィールド確定用変数
  static int load_check = 0;//ボトル装填の動作確認用変数
  static int fire_check = 0;//射角決定の動作確認用変数
  static int pull_check = 0;//バネの動作確認用変数
  static int throw_check2 = 0;//一回射出終了の確認用変数
  static int throw_check = 0;
  static int def_ang_check = 0;
  static int def_ang_check2 = 0;
  static int load_check2 = 0;
  static int horizontal_check = 0;
  static int manual_load = 0;
  static int man_load_wait = 0;
  static int man_load_check = 0;
  static int auto_stop = 0;
  static int zero_angle = 0;
  static int pull_check2 = 0;


  ////////////巻取り理想値追加変数//////////////////////////
  static int pul_spr_support = 0;


  ////////////動作待機用カウント変数///////////////////////////////
  static int throw_wait = 0; 
  static int fire_wait = 0;
  static int auto_wait = 0;
  static int throw_count = 0;
  static int pull_count = 0;

  static int man_tes_mode = 0;
  static char auto_mode[2] = {' ',' '};

  //射角の配列を定義
  int fire_angle_arrey[2][BOTTLE_COUNT] = {{FIRST_FIRE_ANGLE_1,SECOND_FIRE_ANGLE_1,THIRD_FIRE_ANGLE_1,FOURTH_FIRE_ANGLE_1,FIFTH_FIRE_ANGLE_1,SIXTH_FIRE_ANGLE_1,SEVENTH_FIRE_ANGLE_1,EIGHTH_FIRE_ANGLE_1,NINETH_FIRE_ANGLE_1,TENTH_FIRE_ANGLE_1},
					   {FIRST_FIRE_ANGLE_2,SECOND_FIRE_ANGLE_2,THIRD_FIRE_ANGLE_2,FOURTH_FIRE_ANGLE_2,FIFTH_FIRE_ANGLE_2,SIXTH_FIRE_ANGLE_2,SEVENTH_FIRE_ANGLE_2,EIGHTH_FIRE_ANGLE_2,NINETH_FIRE_ANGLE_2,TENTH_FIRE_ANGLE_2}};

  //バネ引く強さの配列を定義
  int pull_spring_arrey[2][BOTTLE_COUNT] = {{FIRST_PULL_SPRING_1,SECOND_PULL_SPRING_1,THIRD_PULL_SPRING_1,FOURTH_PULL_SPRING_1,FIFTH_PULL_SPRING_1,SIXTH_PULL_SPRING_1,SEVENTH_PULL_SPRING_1,EIGHTH_PULL_SPRING_1,NINETH_PULL_SPRING_1,TENTH_PULL_SPRING_1},
					    
					    {FIRST_PULL_SPRING_2,SECOND_PULL_SPRING_2,THIRD_PULL_SPRING_2,FOURTH_PULL_SPRING_2,FIFTH_PULL_SPRING_2,SIXTH_PULL_SPRING_2,SEVENTH_PULL_SPRING_2,EIGHTH_PULL_SPRING_2,NINETH_PULL_SPRING_2,TENTH_PULL_SPRING_2}};
 
  
  load_photo_looked = _IS_PRESSED_LOAD_PHOTO();
  
  
  if(status == -1 && _IS_PRESSED_POSITION_TOGLE()/*スイッチが右フィールドの時*/){
    start_position = 0;
    position_look = 'L';
 
  }else if(status == -1 && !_IS_PRESSED_POSITION_TOGLE()/*スイッチが左フィールドの時*/){
    start_position = 1;
    position_look  = 'R';
  }

  if(start_position == 0 || start_position == 1){
    position_set = 1;
  }

  if(status < 0){
    if(__RC_ISPRESSED_TRIANGLE(g_rc_data)){
      man_tes_mode = 1;
    }else if(__RC_ISPRESSED_SQARE(g_rc_data)){
      man_tes_mode = 2;
    }else if(__RC_ISPRESSED_CROSS(g_rc_data)){
      man_tes_mode = 3;
    }else if(__RC_ISPRESSED_CIRCLE(g_rc_data)){
      man_tes_mode = 4;
    }
    
    if(man_tes_mode == 1){
      auto_mode[0] = ' ';
      auto_mode[1] = 'P';
    }else if(man_tes_mode == 2){
      auto_mode[0] = ' ';
      auto_mode[1] = 'L';
    }else if(man_tes_mode == 3){
      auto_mode[0] = ' ';
      auto_mode[1] = 'R';
    }else if(man_tes_mode == 4){
      auto_mode[0] = ' ';
      auto_mode[1] = 'F';
    }
    
      
    if(man_tes_mode == 1){
      if(__RC_ISPRESSED_R1(g_rc_data)){
	pull_motor = 1;
      }else if(__RC_ISPRESSED_L1(g_rc_data)){
	pull_motor = 2;
      }else{
	pull_motor = 0;
      }
    }else if(man_tes_mode == 2){
      if(__RC_ISPRESSED_R1(g_rc_data)){
	load_bottle = 2;
      }else if(__RC_ISPRESSED_L1(g_rc_data)){
	load_bottle = 1;
      }else{
	load_bottle = 0;
      }
    }else if(man_tes_mode == 3){
      if(__RC_ISPRESSED_R1(g_rc_data)){
	throw_motor = 1;
      }else if(__RC_ISPRESSED_L1(g_rc_data)){
	throw_motor = 2;
      }else{
	throw_motor = 0;
      }
    }else if(man_tes_mode == 4){
      if(__RC_ISPRESSED_R1(g_rc_data)){
	fire_motor = 2;
      }else if(__RC_ISPRESSED_L1(g_rc_data)){
	fire_motor = 1;
      }else{
	fire_motor = 0;
      }
    }
  }

  //[R2,L1]でスタート 
  if(status < 0 && position_set == 1 && _IS_PRESSED_MANLOAD_TOGLE())
    {
      status = 0;
    }
 
 


  if(status >= 0){

  
    fire_angle = fire_angle_arrey[start_position][status];//start_posishonとstatusで射角を決定
  
    pull_spring = pul_spr_support + pull_spring_arrey[start_position][status];//start_posishonとstatusでバネを引く強さを決定
     
    if(!_IS_PRESSED_MANLOAD_TOGLE()){
      auto_stop = 1;
    }else if(_IS_PRESSED_MANLOAD_TOGLE()){
      auto_stop = 0;
    }
    
    if(auto_stop == 1){
      pull_motor = 0;
      load_bottle = 0;
      throw_motor = 0;
      fire_motor = 0;
    }

      
    if(auto_stop == 0){
      
      if(fire_angle > 0){
	default_angle = DEFAULT_ANGLE1;
      }else if(fire_angle < 0){
	default_angle = -DEFAULT_ANGLE1;
      }

      if(fire_angle > 0){
	zero_angle = ZERO_ANGLE;
      }else if(fire_angle < 0){
	zero_angle = -ZERO_ANGLE;
      }

      if(status <= 0){
	auto_mode[0] = ' ';
	auto_mode[1] = 'A';
      }
      
      //土台defaultに戻す
      if(def_ang_check == 0 && status >= 0){
    
	if(encoder_fire < default_angle){
	  if(encoder_fire < default_angle -ANGLE_MARGIN){
	    fire_motor = 2;

	  }else{
	    fire_motor = 0;
	    def_ang_check = 1;
	  }
	}else if(encoder_fire > default_angle){
	  if(encoder_fire > default_angle +ANGLE_MARGIN){
	    fire_motor = 1;
	  }else{
	    fire_motor = 0;
	    def_ang_check = 1;
	  }
	}else{
	  fire_motor = 0;
	  def_ang_check = 1;
	}
      }

  
      //地面と並行まで腕を引く
      if(horizontal_check == 0  && def_ang_check == 1 && throw_check == 1){
	if(encoder_pull > pul_spr_support + HORIZONTAL){
	  pull_motor = 1;
	}else{
	  pull_motor = 0;
	  horizontal_check = 1;
	}
      }



      //土台０度に戻す
      if(def_ang_check2 == 0 && status >= 0 && def_ang_check == 1 && horizontal_check == 1 && throw_check == 1){
    
	if(encoder_fire < 0){
	  if(encoder_fire < zero_angle -ANGLE_MARGIN){
	    fire_motor = 2;

	  }else{
	    fire_motor = 0;
	    def_ang_check2 = 1;
	  }
	}else if(encoder_fire > 0){
	  if(encoder_fire > zero_angle +ANGLE_MARGIN){
	    fire_motor = 1;
	  }else{
	    fire_motor = 0;
	    def_ang_check2 = 1;
	  }
	}else{
	  fire_motor = 0;
	  def_ang_check2 = 1;
	}
      }

      //巻取り機構を開放する奴を初期値に戻す
      if(throw_check == 0){
	
	if(status == 0){
	  throw_check = 1;
	  throw_count = THROW_COUNT;
	}
	if(status > 0){
	  if(throw_count < THROW_COUNT){
	    throw_count ++;
	    throw_motor = 2;
	  }
	  if(throw_count == THROW_COUNT){
	    throw_motor = 0;
	    throw_check = 1;
	  }
	}
      }
      //装填の制御
      if(load_check == 0 && horizontal_check == 1 && def_ang_check == 1 && def_ang_check2 == 1 && throw_check == 1){
	load_check2 = 1;
 
	if(load_photo_looked == 1 && load_photo_count == load_photo_support){
	  load_photo_count ++;
	}
	if(load_photo_looked == 0){
	  load_photo_support = load_photo_count;
	}
	switch(status){
	case 0:
	  load_count_go = 1;
	  load_count_back = 2;
	  load_count_stop = 3;
	  break;

	case 1:
	  load_count_go = 3;
	  load_count_back = 5;
	  load_count_stop = 7;
	  break;

	case 2:
	  load_count_go = 7;
	  load_count_back = 10;
	  load_count_stop = 13;
	  break;

	case 3:
	  load_count_go = 13;
	  load_count_back = 17;
	  load_count_stop = 21;
	  break;

	case 4:
	  load_count_go = 21;
	  load_count_back = 22;
	  load_count_stop = 23;
	  break;

	case 5:
	  load_count_go = 23;
	  load_count_back = 25;
	  load_count_stop = 27;
	  break;

	case 6:
	  load_count_go = 27;
	  load_count_back = 30;
	  load_count_stop = 33;
	  break;

	case 7:
	  load_count_go = 33;
	  load_count_back = 37;
	  load_count_stop = 41;
	  break;

	case 8:
	  load_count_go = 41;
	  load_count_back = 42;
	  load_count_stop = 43;
	  break;

        case 9:
	  load_count_go = 43;
	  load_count_back = 45;
	  load_count_stop = 47;
	  break;
	  

	}
	  
	  
	if(load_photo_count == load_count_go){
	  load_bottle = 2;
	}
   
	if(load_photo_count == load_count_back){
	  load_bottle = 1;
	}

	if(load_photo_count == load_count_stop){
	  load_bottle = 0;
	  load_check = 1;
	}	
      }

      //射出方向決定
      if(fire_check == 0 && load_check == 1 && load_check2 == 1 &&  horizontal_check == 1 && def_ang_check == 1 && def_ang_check2 == 1 && throw_check == 1){

	if(fire_wait < FIRE_WAIT){
	  fire_wait ++;
	}else if(fire_wait == FIRE_WAIT){
    
	  if(fire_angle > 0){
	    if(encoder_fire < fire_angle - ANGLE_MARGIN){
	      fire_motor = 2;
	    }else{
	      fire_motor = 0;
	      fire_check = 1;
	    } 

	  }else if(fire_angle < 0){
	    if(encoder_fire > fire_angle + ANGLE_MARGIN){
	      fire_motor = 1;
	    }else{
	      fire_motor = 0;
	      fire_check = 1;
	    } 
	  }
	}
      }

      //目標値までバネ引く
      if(/*pull_check == 0 && */fire_check == 1 && load_check == 1 && load_check2 == 1 &&  horizontal_check == 1 && def_ang_check == 1 && def_ang_check2 == 1 &&  throw_check == 1){
	if(HORIZONTAL > pull_spring_arrey[start_position][status]){
	  if(encoder_pull > pull_spring){      
	    pull_motor = 1;
	  }else{
	    pull_motor = 0;
	    pull_check = 1;
	  }
	  if(pull_check == 1){
	    if(pull_count < PULL_COUNT){
	      pull_count ++;
	      pull_motor = 2;
	    }else{
	      pull_motor = 0;
              pull_check2 = 1;
	    }
	  }
	   

	}else if(HORIZONTAL < pull_spring_arrey[start_position][status]){
	  if(encoder_pull < pull_spring - PULL_UP_MARGIN){      
	    pull_motor = 2;
	  }else{
	    pull_motor = 0;
	    pull_check = 1;
	  }
	  if(pull_check == 1){
	    if(pull_count < PULL_COUNT){
	      pull_count ++;
	      pull_motor = 1;
	    }else{
	      pull_motor = 0;
              pull_check2 = 1;
	    }
	  }
	}
      }
 

      //射出の制御
      if(throw_check2 == 0 && pull_check == 1 && pull_check2 == 1 && fire_check == 1 && load_check == 1 && load_check2 == 1 && horizontal_check == 1 && def_ang_check == 1 && def_ang_check2 == 1 && throw_check == 1){
	
	if(throw_wait < THROW_WAIT){
	  throw_wait ++;
	}else if(throw_wait == THROW_WAIT){

	  if(throw_count > 0){
	    throw_count --;
	    throw_motor = 1;
	  }
	  if(throw_count == 0){
	    throw_motor = 0;
	    throw_check2 = 1;
	  }
	}
      }
    }


    //一本投射後の処理
    if(throw_check2 == 1 && pull_check == 1 && fire_check == 1 && load_check == 1 && load_check2 == 1 && horizontal_check == 1 && def_ang_check == 1 && def_ang_check2 == 1 && throw_check == 1){
  
      if(auto_wait < AUTO_WAIT){
	auto_wait ++;
      }
 
      if(auto_wait == AUTO_WAIT){
	if(((status + 1) % LOAD_MAN_TURN) == 0 && manual_load == 0){
	  if(_IS_PRESSED_MANLOAD_TOGLE()){
	    auto_stop = 1;
	  }
	  if(auto_stop == 1 && !_IS_PRESSED_MANLOAD_TOGLE()){
	    man_load_check = 1;
	  }

	  if(man_load_check == 1 && _IS_PRESSED_MANLOAD_TOGLE()){
	    if(man_load_wait < MAN_LOAD_WAIT){
	      man_load_wait ++;
	    }
	    if(man_load_wait == MAN_LOAD_WAIT){
	      auto_stop = 0;
	      man_load_check = 0;
	      auto_stop = 0;
	      man_load_wait = 0;
	      manual_load = 1;
	    }
	  }


	  
	}else{
	  manual_load = 1;
	}
  
	//射出処理一回分の終了　それにあたっての各変数の初期化
      

	if(manual_load == 1){
      
	    
	  fire_check = 0;
	  def_ang_check = 0;
	  def_ang_check2 = 0;
	  fire_wait = 0;

	  pull_check = 0;
          pull_check2 = 0;
	  horizontal_check = 0;
	  pul_spr_support = encoder_pull;
	  pull_count = 0;
	    
	  throw_check = 0;
	  throw_check2 = 0;
	  throw_wait = 0;
	    
	  load_check = 0;
	  load_check2 = 0;
      
	  manual_load = 0;
	  status ++;
	  auto_wait = 0;
	}
      }
    }
  }
  
  if(status == BOTTLE_COUNT){
    auto_stop = 1;
    load_bottle = 0;
    pull_motor = 0;
    throw_motor = 0;
    fire_motor = 0;
   
  } 

  MW_printf("[%5d]",encoder_pull); 
  MW_printf("\r[%c][%s]STATUS[%d]LOCHE[%d]HOCHE[%d]THCHE[%d]DACHE[%d]DACH2[%d]FICHE[%d]PUCHE[%d]LOPHO[%d]THCOU[%d]",position_look,auto_mode,status,load_check,
	    horizontal_check,throw_check,def_ang_check,
	    def_ang_check2,fire_check,pull_check,_IS_PRESSED_LOAD_PHOTO(),throw_count);
   
  return EXIT_SUCCESS;
}//　auto_con関数の終了

// 射角変更
static
int bodyRotate(void){
  const tc_const_t body_tcon = {
    .inc_con = 50,
    .dec_con = 500,
  };
  int i;

  /* 射角変更のduty */
  static int body_target = 0;
  const int turn_right_duty = FIRE_DUTY;
  const int turn_left_duty = -FIRE_DUTY;

  /* コントローラのボタンは押されてるか（？） */
  if(fire_motor == 1){   
    body_target = turn_right_duty;

  }else if(fire_motor == 2){
    body_target = turn_left_duty;
    
  }else{
    body_target = D_MMOD_BRAKE;
  }
  for(i = 0;i < 5;i++){
    trapezoidCtrl(body_target,&g_md_h[MECHA1_MD1],&body_tcon);
  }
  
  return EXIT_SUCCESS;
}

/* バネ引く */
static
int armSystem(void){
  const tc_const_t arm_tcon ={
    .inc_con = 100,
    .dec_con = 400,
  };

  /* バネ引くのduty */
  int arm_target = 0;
  const int arm_cw_duty = -ARM_PULL_DUTY;
  const int arm_ccw_duty = ARM_PULL_DUTY;
	
  //コントローラのボタンは押されているか（？）  
  if(pull_motor == 1){
    arm_target = arm_cw_duty;
  } else if(pull_motor == 2){
    arm_target = arm_ccw_duty;
  }else{
    arm_target = D_MMOD_BRAKE;
  }

  trapezoidCtrl(arm_target,&g_md_h[MECHA1_MD2],&arm_tcon);
 

  return EXIT_SUCCESS;
}

/* 装填機構 */
static
int put_bottle(void){
  const tc_const_t load_tcon ={
    .inc_con = 100,
    .dec_con = 600,
  };

  /* 装填機構のduty */
  int load_target = 0;
  const int load_duty = -LOAD_DUTY;
  
  int i;

  //コントローラのボタンは押されているか（？）  
  if(load_bottle == 1){
    load_target = load_duty;
  
  }else if(load_bottle == 2){
    load_target = -load_duty;
  }else{
    load_target = D_MMOD_BRAKE;
  }
  for(i=0;i<5;i++){
    trapezoidCtrl(load_target,&g_md_h[MECHA1_MD0],&load_tcon);
    /* コントローラのボタンは押されてるか */
  }

  return EXIT_SUCCESS;
}

// 射出機構 
static 
int throw(void){ 
  const tc_const_t throw_tcon ={
    .inc_con = 300,
    .dec_con = 500,
  };
     
  // 射出機構のduty
  int throw_target = 0;
  const int throw_duty = THROW_DUTY;

  //コントローラのボタンは押されているか（？）
  if(throw_motor == 1){ 
    throw_target = throw_duty;
  }else if(throw_motor == 2){ 
    throw_target = -throw_duty;
  }else{
    throw_target = D_MMOD_BRAKE;
  }
  trapezoidCtrl(throw_target,&g_md_h[MECHA1_MD3],&throw_tcon);
  // コントローラのボタンは押されてるか 
 
  return EXIT_SUCCESS;
} 


static int LEDSystem(void){
  switch( start_position ){
  case 0:
    g_led_mode = lmode_1;
    break;
  case 1:
    g_led_mode = lmode_3;
    break;
  default:
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

static
int32_t I2C_Encoder(int encoder_num, EncoderOperation_t operation){
  int32_t value = 0;
  int32_t temp_value;
  static int32_t adjust[4] = {0,0,0,0};

  switch(encoder_num){
  case 0:
    temp_value = g_ss_h[I2C_ENCODER_1].data[0] + (g_ss_h[I2C_ENCODER_1].data[1] << 8) + (g_ss_h[I2C_ENCODER_1].data[2] << 16) + (g_ss_h[I2C_ENCODER_1].data[3] << 24);
    value = temp_value + adjust[0];
    if(operation == GET_ENCODER_VALUE){
      break;
    }else if(operation == RESET_ENCODER_VALUE){
      adjust[0] = -temp_value;
      value = temp_value + adjust[0];
    }
    break;
    
  case 1:
    temp_value = g_ss_h[I2C_ENCODER_1].data[4] + (g_ss_h[I2C_ENCODER_1].data[5] << 8) + (g_ss_h[I2C_ENCODER_1].data[6] << 16) + (g_ss_h[I2C_ENCODER_1].data[7] << 24);
    value = temp_value + adjust[1];
    if(operation == GET_ENCODER_VALUE){
      break;
    }else if(operation == RESET_ENCODER_VALUE){
      adjust[1] = -temp_value;
      value = temp_value + adjust[1];
    }
    break;
  }
  
  return value;
}
