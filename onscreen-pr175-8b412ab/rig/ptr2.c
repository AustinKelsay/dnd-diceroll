/* Scriptable virtual pointer (zwlr_virtual_pointer_v1): move, click, drag with any button, wheel.
   usage: ptr2 W H op args... [op args...]
   ops: m X Y | c [btn] | bd btn | bu btn | drag btn X1 Y1 X2 Y2 STEPS | w N (N>0 wheel down, N<0 up) | s MS
   btn is a Linux code: 272 left, 273 right, 274 middle. Coordinates are absolute in output pixels. */
#include <wayland-client.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include "virtual-pointer.h"
static struct zwlr_virtual_pointer_manager_v1 *manager;
static struct wl_display *d;
static struct zwlr_virtual_pointer_v1 *p;
static uint32_t W, H;
static uint32_t now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint32_t)(t.tv_sec*1000+t.tv_nsec/1000000);}
static void global(void*x,struct wl_registry*r,uint32_t n,const char*i,uint32_t v){if(!strcmp(i,"zwlr_virtual_pointer_manager_v1"))manager=wl_registry_bind(r,n,&zwlr_virtual_pointer_manager_v1_interface,v<2?v:2);}
static void removed(void*x,struct wl_registry*r,uint32_t n){}
static void flush(void){zwlr_virtual_pointer_v1_frame(p);wl_display_roundtrip(d);}
static void mv(int x,int y){zwlr_virtual_pointer_v1_motion_absolute(p,now(),x,y,W,H);flush();}
static void btn(int b,int down){zwlr_virtual_pointer_v1_button(p,now(),b,down?WL_POINTER_BUTTON_STATE_PRESSED:WL_POINTER_BUTTON_STATE_RELEASED);flush();}
static void ms(int m){wl_display_roundtrip(d);usleep(m*1000);}
int main(int argc,char**argv){
  if(argc<4)return 2; W=atoi(argv[1]);H=atoi(argv[2]);
  d=wl_display_connect(NULL);if(!d){fprintf(stderr,"no display\n");return 3;}
  struct wl_registry*r=wl_display_get_registry(d);const struct wl_registry_listener l={global,removed};
  wl_registry_add_listener(r,&l,NULL);wl_display_roundtrip(d);if(!manager){fprintf(stderr,"no virtual pointer manager\n");return 4;}
  p=zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager,NULL);
  for(int i=3;i<argc;){
    const char*o=argv[i++];
    if(!strcmp(o,"m")){mv(atoi(argv[i]),atoi(argv[i+1]));i+=2;ms(60);}
    else if(!strcmp(o,"c")){int b=272;if(i<argc&&atoi(argv[i])>=272&&atoi(argv[i])<=274&&strlen(argv[i])==3)b=atoi(argv[i++]);btn(b,1);ms(80);btn(b,0);ms(60);}
    else if(!strcmp(o,"bd")){btn(atoi(argv[i++]),1);ms(60);}
    else if(!strcmp(o,"bu")){btn(atoi(argv[i++]),0);ms(60);}
    else if(!strcmp(o,"drag")){int b=atoi(argv[i]),x1=atoi(argv[i+1]),y1=atoi(argv[i+2]),x2=atoi(argv[i+3]),y2=atoi(argv[i+4]),n=atoi(argv[i+5]);i+=6;
      mv(x1,y1);ms(100);btn(b,1);ms(100);for(int k=1;k<=n;k++){mv(x1+(x2-x1)*k/n,y1+(y2-y1)*k/n);ms(25);}ms(100);btn(b,0);ms(80);}
    else if(!strcmp(o,"w")){int n=atoi(argv[i++]);int s=n<0?-1:1;for(int k=0;k<abs(n);k++){
      zwlr_virtual_pointer_v1_axis_source(p,WL_POINTER_AXIS_SOURCE_WHEEL);
      zwlr_virtual_pointer_v1_axis_discrete(p,now(),WL_POINTER_AXIS_VERTICAL_SCROLL,wl_fixed_from_int(15*s),s);
      flush();ms(120);}}
    else if(!strcmp(o,"s")){ms(atoi(argv[i++]));}
    else if(!strcmp(o,"sh")){system(argv[i++]);}
    else if(!strcmp(o,"wf")){const char*f=argv[i++];while(access(f,F_OK)!=0){ms(20);} unlink(f);}
    else {fprintf(stderr,"bad op %s\n",o);return 5;}
  }
  zwlr_virtual_pointer_v1_destroy(p);zwlr_virtual_pointer_manager_v1_destroy(manager);wl_display_roundtrip(d);wl_display_disconnect(d);return 0;}
