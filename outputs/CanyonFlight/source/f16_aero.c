/* v33 F-16 aerodynamics and engine.

   The aerodynamic and thrust tables are the condensed F-16 model published
   by Stevens & Lewis ("Aircraft Control and Simulation"), which they derived
   from the NASA Langley wind-tunnel data in NASA TP-1538 (Nguyen et al.,
   1979, public domain). Valid for -10..45 deg angle of attack, +-30 deg
   sideslip, subsonic speeds; the interpolation extrapolates at the edges
   as the book's routines do. Reference geometry: S = 300 ft2, b = 30 ft,
   cbar = 11.32 ft, reference cg 0.35 cbar (this model flies at 0.35).

   Sign conventions of the tables: elevator, aileron and rudder in degrees,
   positive trailing edge down / right aileron down / rudder trailing edge
   left. The simulator's inputs are positive for nose up, roll right and
   nose right, so they are negated below. */
#include "f16_aero.h"
#define DEG2RAD .0174532925f
static const float ALPHA0=-10,ALPHA_STEP=5;          /* 12 points, -10..45 */
static const float CXT[5][12]={                       /* elevator -24..24 */
 {-.099f,-.081f,-.081f,-.063f,-.025f,.044f,.097f,.113f,.145f,.167f,.174f,.166f},
 {-.048f,-.038f,-.040f,-.021f,.016f,.083f,.127f,.137f,.162f,.177f,.179f,.167f},
 {-.022f,-.020f,-.021f,-.004f,.032f,.094f,.128f,.130f,.154f,.161f,.155f,.138f},
 {-.040f,-.038f,-.039f,-.025f,.006f,.062f,.087f,.085f,.100f,.110f,.104f,.091f},
 {-.083f,-.073f,-.076f,-.072f,-.046f,.012f,.024f,.025f,.043f,.053f,.047f,.040f}};
static const float CZT[12]={.770f,.241f,-.100f,-.416f,-.731f,-1.053f,-1.366f,-1.646f,-1.917f,-2.120f,-2.248f,-2.229f};
static const float CMT[5][12]={
 {.205f,.168f,.186f,.196f,.213f,.251f,.245f,.238f,.252f,.231f,.198f,.192f},
 {.081f,.077f,.107f,.110f,.110f,.141f,.127f,.119f,.133f,.108f,.081f,.093f},
 {-.046f,-.020f,-.009f,-.005f,-.006f,.010f,.006f,-.001f,.014f,.000f,-.013f,.032f},
 {-.174f,-.145f,-.121f,-.127f,-.129f,-.102f,-.097f,-.113f,-.087f,-.084f,-.069f,-.006f},
 {-.259f,-.202f,-.184f,-.193f,-.199f,-.150f,-.160f,-.167f,-.104f,-.076f,-.041f,-.005f}};
static const float CLT[7][12]={                       /* |beta| 0..30 */
 {0,0,0,0,0,0,0,0,0,0,0,0},
 {-.001f,-.004f,-.008f,-.012f,-.016f,-.022f,-.022f,-.021f,-.015f,-.008f,-.013f,-.015f},
 {-.003f,-.009f,-.017f,-.024f,-.030f,-.041f,-.045f,-.040f,-.016f,-.002f,-.010f,-.019f},
 {-.001f,-.010f,-.020f,-.030f,-.039f,-.054f,-.057f,-.054f,-.023f,-.006f,-.014f,-.027f},
 {.000f,-.010f,-.022f,-.034f,-.047f,-.060f,-.069f,-.067f,-.033f,-.036f,-.035f,-.035f},
 {.007f,-.010f,-.023f,-.034f,-.049f,-.063f,-.081f,-.079f,-.060f,-.058f,-.062f,-.059f},
 {.009f,-.011f,-.023f,-.037f,-.050f,-.068f,-.089f,-.088f,-.091f,-.076f,-.077f,-.076f}};
static const float CNT[7][12]={
 {0,0,0,0,0,0,0,0,0,0,0,0},
 {.018f,.019f,.018f,.019f,.019f,.018f,.013f,.007f,.004f,-.014f,-.017f,-.033f},
 {.038f,.042f,.042f,.042f,.043f,.039f,.030f,.017f,.004f,-.035f,-.047f,-.057f},
 {.056f,.057f,.059f,.058f,.058f,.053f,.032f,.012f,.002f,-.046f,-.071f,-.073f},
 {.064f,.077f,.076f,.074f,.073f,.057f,.029f,.007f,.012f,-.034f,-.065f,-.041f},
 {.074f,.086f,.093f,.089f,.080f,.062f,.049f,.022f,.028f,-.012f,-.002f,-.013f},
 {.079f,.090f,.106f,.106f,.096f,.080f,.068f,.030f,.064f,.015f,.011f,-.001f}};
static const float DLDA[7][12]={                      /* beta -30..30 step 10 */
 {-.041f,-.052f,-.053f,-.056f,-.050f,-.056f,-.082f,-.059f,-.042f,-.038f,-.027f,-.017f},
 {-.041f,-.053f,-.053f,-.053f,-.050f,-.051f,-.066f,-.043f,-.038f,-.027f,-.023f,-.016f},
 {-.042f,-.053f,-.052f,-.051f,-.049f,-.049f,-.043f,-.035f,-.026f,-.016f,-.018f,-.014f},
 {-.040f,-.052f,-.051f,-.052f,-.048f,-.048f,-.042f,-.037f,-.031f,-.026f,-.017f,-.012f},
 {-.043f,-.049f,-.048f,-.049f,-.043f,-.042f,-.042f,-.036f,-.025f,-.021f,-.016f,-.011f},
 {-.044f,-.048f,-.048f,-.047f,-.042f,-.041f,-.020f,-.028f,-.013f,-.014f,-.011f,-.010f},
 {-.043f,-.049f,-.047f,-.045f,-.042f,-.037f,-.003f,-.013f,-.010f,-.003f,-.007f,-.008f}};
static const float DLDR[7][12]={
 {.005f,.017f,.014f,.010f,-.005f,.009f,.019f,.005f,-.000f,-.005f,-.011f,.008f},
 {.007f,.016f,.014f,.014f,.013f,.009f,.012f,.005f,.000f,.004f,.009f,.007f},
 {.013f,.013f,.011f,.012f,.011f,.009f,.008f,.005f,-.002f,.005f,.003f,.005f},
 {.018f,.015f,.015f,.014f,.014f,.014f,.014f,.015f,.013f,.011f,.006f,.001f},
 {.015f,.014f,.013f,.013f,.012f,.011f,.011f,.010f,.008f,.008f,.007f,.003f},
 {.021f,.011f,.010f,.011f,.010f,.009f,.008f,.010f,.006f,.005f,.000f,.001f},
 {.023f,.010f,.011f,.011f,.011f,.010f,.008f,.010f,.006f,.014f,.020f,.000f}};
static const float DNDA[7][12]={
 {.001f,-.027f,-.017f,-.013f,-.012f,-.016f,.001f,.017f,.011f,.017f,.008f,.016f},
 {.002f,-.014f,-.016f,-.016f,-.014f,-.019f,-.021f,.002f,.012f,.016f,.015f,.011f},
 {-.006f,-.008f,-.006f,-.006f,-.005f,-.008f,-.005f,.007f,.004f,.007f,.006f,.006f},
 {-.011f,-.011f,-.010f,-.009f,-.008f,-.006f,.000f,.004f,.007f,.010f,.004f,.010f},
 {-.015f,-.015f,-.014f,-.012f,-.011f,-.008f,-.002f,.002f,.006f,.012f,.011f,.011f},
 {-.024f,-.010f,-.004f,-.002f,-.001f,.003f,.014f,.006f,-.001f,.004f,.004f,.006f},
 {-.022f,.002f,-.003f,-.005f,-.003f,-.001f,-.009f,-.009f,-.001f,.003f,-.002f,.001f}};
static const float DNDR[7][12]={
 {-.018f,-.052f,-.052f,-.052f,-.054f,-.049f,-.059f,-.051f,-.030f,-.037f,-.026f,-.013f},
 {-.028f,-.051f,-.043f,-.046f,-.045f,-.049f,-.057f,-.052f,-.030f,-.033f,-.030f,-.008f},
 {-.037f,-.041f,-.038f,-.040f,-.040f,-.038f,-.037f,-.030f,-.027f,-.024f,-.019f,-.013f},
 {-.048f,-.045f,-.045f,-.045f,-.044f,-.045f,-.047f,-.048f,-.049f,-.045f,-.033f,-.016f},
 {-.043f,-.044f,-.041f,-.041f,-.040f,-.038f,-.034f,-.035f,-.035f,-.029f,-.022f,-.009f},
 {-.052f,-.034f,-.036f,-.036f,-.035f,-.028f,-.024f,-.023f,-.020f,-.016f,-.010f,-.014f},
 {-.062f,-.034f,-.027f,-.028f,-.027f,-.027f,-.023f,-.023f,-.019f,-.009f,-.025f,-.010f}};
/* Damping derivatives vs alpha: CXq CYr CYp CZq Clr Clp Cmq Cnr Cnp. */
static const float DAMP[9][12]={
 {-.267f,-.110f,.308f,1.34f,2.08f,2.91f,2.76f,2.05f,1.50f,1.49f,1.83f,1.21f},
 {.882f,.852f,.876f,.958f,.962f,.974f,.819f,.483f,.590f,1.21f,-.493f,-1.04f},
 {-.108f,-.108f,-.188f,.110f,.258f,.226f,.344f,.362f,.611f,.529f,.298f,-2.27f},
 {-8.80f,-25.8f,-28.9f,-31.4f,-31.2f,-30.7f,-27.7f,-28.2f,-29.0f,-29.8f,-38.3f,-35.3f},
 {-.126f,-.026f,.063f,.113f,.208f,.230f,.319f,.437f,.680f,.100f,.447f,-.330f},
 {-.360f,-.359f,-.443f,-.420f,-.383f,-.375f,-.329f,-.294f,-.230f,-.210f,-.120f,-.100f},
 {-7.21f,-.540f,-5.23f,-5.26f,-6.11f,-6.64f,-5.69f,-6.00f,-6.20f,-6.40f,-6.60f,-6.00f},
 {-.380f,-.363f,-.378f,-.386f,-.370f,-.453f,-.550f,-.582f,-.595f,-.637f,-1.02f,-.840f},
 {.061f,.052f,.052f,-.012f,-.013f,-.024f,.050f,.150f,.130f,.158f,.240f,.150f}};
/* Thrust (lbf): rows Mach 0..1 step .2, columns altitude 0..50,000 ft step 10,000. */
static const float IDLE[6][6]={{1060,670,880,1140,1500,1860},{635,425,690,1010,1330,1700},{60,25,345,755,1130,1525},
 {-1020,-710,-300,350,910,1360},{-2700,-1900,-1300,-247,600,1100},{-3600,-1400,-595,-342,-200,700}};
static const float MIL[6][6]={{12680,9150,6200,3950,2450,1400},{12680,9150,6313,4040,2470,1400},{12610,9312,6610,4290,2600,1560},
 {12640,9839,7090,4660,2840,1660},{12390,10176,7750,5320,3250,1930},{11680,9848,8050,6100,3800,2310}};
static const float MAXT[6][6]={{20000,15000,10800,7000,4000,2500},{21420,15700,11225,7323,4435,2600},{22700,16860,12250,8154,5000,2835},
 {24240,18910,13760,9285,5700,3215},{26070,21075,15975,11115,6860,3950},{28886,23319,18300,13484,8642,5057}};

/* Index with the book's extrapolation: k = clamp(floor) in [lo, hi-1]. */
static void slot(float x,int n,int *k,float *f){int i=(int)floorf(x);if(i<0)i=0;if(i>n-2)i=n-2;*k=i;*f=x-i;}
static float alphaIndex(float alphaDeg){return (alphaDeg-ALPHA0)/ALPHA_STEP;}
static float table1(const float t[12],float a){int k;float f;slot(alphaIndex(a),12,&k,&f);return t[k]+f*(t[k+1]-t[k]);}
static float table2(const float t[][12],int rows,float a,float row){
 int k,m;float f,g;slot(alphaIndex(a),12,&k,&f);slot(row,rows,&m,&g);
 float lo=t[m][k]+f*(t[m][k+1]-t[m][k]),hi=t[m+1][k]+f*(t[m+1][k+1]-t[m+1][k]);return lo+g*(hi-lo);
}
static float thrustTable(const float t[6][6],float mach,float altFt){
 int m,h;float fm,fh;slot(mach/.2f,6,&m,&fm);slot(altFt/10000,6,&h,&fh);
 float lo=t[m][h]+fh*(t[m][h+1]-t[m][h]),hi=t[m+1][h]+fh*(t[m+1][h+1]-t[m+1][h]);return lo+fm*(hi-lo);
}
/* Throttle 0..1 -> power percent (book's TGEAR): 0..50 military, 50..100 afterburner. */
float f16Power(float throttle){throttle=clampf(throttle,0,1);return throttle<=.77f?64.94f*throttle:217.38f*throttle-117.38f;}
float f16ThrustNewtons(float throttle,float mach,float altitude){
 float power=f16Power(throttle),altFt=fmaxf(altitude,0)*3.28084f;mach=clampf(mach,0,1);
 float idle=thrustTable(IDLE,mach,altFt),mil=thrustTable(MIL,mach,altFt),lbf;
 if(power<50)lbf=idle+(mil-idle)*power*.02f;
 else lbf=mil+(thrustTable(MAXT,mach,altFt)-mil)*(power-50)*.02f;
 return lbf*4.44822f;
}
void f16Trim(float v,float rho,float mass,float gamma,float *alpha,float *elevator){
 float qS=.5f*rho*v*v*F16_AREA,W=mass*9.80665f,a=0,el=0;
 for(int it=0;it<40;it++){
  float lo=-1,hi=1;for(int k=0;k<30;k++){el=(lo+hi)/2;if(f16Coefficients(a,0,v,0,0,0,el,0,0).cm>0)hi=el;else lo=el;}
  F16Coefficients c=f16Coefficients(a,0,v,0,0,0,el,0,0);
  a=clampf(a+(qS*c.cz+W*cosf(gamma)*cosf(a))/(qS*3.6f),-.15f,.6f);
 }
 *alpha=a;*elevator=el;
}
F16Coefficients f16Coefficients(float alpha,float beta,float speed,float p,float q,float r,float elevator,float aileron,float rudder){
 F16Coefficients c;const float b=30,cbar=11.32f,xcgr=.35f,xcg=.35f;
 /* v34.06: the tables are looked up inside their range only (-10..45 deg
    alpha, +-30 deg beta). The book's linear extrapolation gave unbounded
    coefficients in a spin (alpha up to 180 deg), and the model diverged.
    Past the edges the table values blend over 15 deg into a flat plate
    (normal force 1.5 sin alpha, nose-down pitch); see the end. */
 float aRaw=alpha/DEG2RAD,bRaw=beta/DEG2RAD;
 float a=clampf(aRaw,-10,45),bd=clampf(bRaw,-30,30);
 float plate=clampf(aRaw>45?(aRaw-45)/15:(-10-aRaw)/15,0,1);
 float el=clampf(-elevator*25,-25,25),ail=clampf(-aileron*21.5f,-21.5f,21.5f),rdr=clampf(-rudder*30,-30,30);
 float vt=fmaxf(speed*3.28084f,30),cq=cbar/(2*vt),bq=b/(2*vt);
 float sb=bd<0?-1.f:1.f,ab=fminf(fabsf(bd),30);
 c.cx=table2(CXT,5,a,el/12+2)+cq*q*table1(DAMP[0],a);
 c.cy=-1.15f*sinf(beta)+.021f*(ail/20)+.086f*(rdr/30)+bq*(table1(DAMP[1],a)*r+table1(DAMP[2],a)*p);
 c.cz=table1(CZT,a)*(1-(bd*DEG2RAD)*(bd*DEG2RAD))*fmaxf(cosf(beta),0)/fmaxf(cosf(bd*DEG2RAD),.01f)-.19f*(el/25)+cq*q*table1(DAMP[3],a);
 float bi=bd/10+3;
 c.cl=sb*table2(CLT,7,a,ab/5)+table2(DLDA,7,a,bi)*(ail/20)+table2(DLDR,7,a,bi)*(rdr/30)
      +bq*(table1(DAMP[4],a)*r+table1(DAMP[5],a)*p);
 c.cm=table2(CMT,5,a,el/12+2)+c.cz*(xcgr-xcg)+cq*q*table1(DAMP[6],a);
 c.cn=sb*table2(CNT,7,a,ab/5)+table2(DNDA,7,a,bi)*(ail/20)+table2(DNDR,7,a,bi)*(rdr/30)
      -c.cy*(xcgr-xcg)*cbar/b+bq*(table1(DAMP[7],a)*r+table1(DAMP[8],a)*p);
 if(plate>0){
  float sa=sinf(alpha),ca=cosf(alpha),dq=cq*q*table1(DAMP[6],a);
  c.cx=c.cx*(1-plate)+plate*(-.1f*ca);
  c.cz=c.cz*(1-plate)+plate*(-1.5f*sa*fmaxf(cosf(beta),0)+cq*q*table1(DAMP[3],a));
  c.cm=c.cm*(1-plate)+plate*(-.35f*sa+dq);
  c.cl*=1-.7f*plate;c.cn*=1-.5f*plate;
 }
 return c;
}
