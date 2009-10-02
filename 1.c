#include <assert.h>
#include <stdio.h>
#include <malloc.h>

#include <sencaml.h>
#include <sencam_def.h>
#include <typtxt.h>
#include <loglevel.h>

#include <GL/glut.h>

const int tx=1376,ty=1040,target=GL_TEXTURE_RECTANGLE_ARB;

void
reshape (int w, int h)
{
  glViewport (0, 0, w, h);
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glOrtho (0, w, 0, h, -500, 500);
  glMatrixMode(GL_MODELVIEW);
}

int hdriver;
struct cam_param params;
unsigned char buf[1376*1040];

void
cam_read()
{
  int
//	type=M_FAST,submode=NORMALFAST,
    type=M_LONG, // 0
    gain=0, // 0 normal, 1 extended gain, 3 low light mode
    submode=
    //NORMALLONG // exposure and readout sequentially
    //VIDEO // no delay, simultaneous exposure and readout, not all trigmodes
    //MECHSHUT // bnc on pci board monitors exposure time, no trigmode possible
    QE_FAST // sequential with short exposure times
	// delay 0..50e6 and expos_width 500..10e6 in ns     
//QE_DOUBLE // two images, started with trig from BNC
    ,
mode=type+gain*256+submode*65536;
  int r=sen_set_coc(hdriver,
		mode, // mode
		0, // trig and start mode
		1, // roixmin
		43, //params.ccdwidth/32+1, // roixmax
		1, // roiymin
		33, //params.ccdheight/32+1, // roiymax
		1, // hbin
		1, // vbin
		"0,30000,-1,-1" // delay and exposure times in ms 0..1000000
		);
  // roix 1..43, roiy 1..33
  // only roiy{min,max} and vbin={1,2,4,8,16} decrease readout times
  printf("ccd: 0x%x\n",params.cam_ccd);
  
  // case 8
  int ccdx,ccdy,actx,acty,bit_pix;
  r=sen_getsizes(hdriver,&ccdx,&ccdy,&actx,&acty,&bit_pix);
  printf("top left: %dx%d\ndimension: %dx%d\n",ccdx,ccdy,actx,acty);
  //actx++; acty++;
  struct cam_values values;
  sen_get_cam_values(hdriver,&values);
  printf("delay %.2fus, exp %.2fus, coc %.2fus\n",
	 values.deltime,values.exptime,values.coctime);
  int bufnr=-1, size=actx*acty*2;
  r=sen_allocate_buffer(hdriver,&bufnr,&size);
  assert(r>=0);
  r=sen_run_coc(hdriver,CAM_SINGLE); // 0 continous, 4 single trigger
  assert(r>=0);
  r=sen_add_buffer_to_list(hdriver,bufnr,size,0,0); //p 37 
  // last params: offset, data (not implemented)
  assert(r>=0);

  int stat;
  do{
    usleep(1000);
    sen_get_buffer_status(hdriver,bufnr,0,&stat,sizeof(stat)); // p 34
  }while((stat & 0x0f02) == 0);
  
  if(stat & 0xf00)
    printf("bufferstatus error-flag is set\n");
  
  if(stat&2){
    static unsigned short*picb12=0;
    if(!picb12)
      picb12=(unsigned short*)malloc(size);
    r=sen_copy_buffer(hdriver,bufnr,picb12,size,0); //p41 last is offset
    int i;
    for(i=0;i<actx*acty;i++)
      buf[i]=picb12[i]/8;
    
    assert(r>=0);
  } else { // something is wrong remove buffer
    assert(0==sen_remove_buffer_from_list(hdriver,bufnr));
  }

  r=sen_stop_coc(hdriver,0);
  assert(r>=0);
  r=sen_free_buffer(hdriver,bufnr);
}

void
draw (void)
{
  glClear(GL_COLOR_BUFFER_BIT);
  glLoadIdentity();
  glColor3d(1,1,1);

  static int new=1;
  if(new){
    cam_read();
    //new=0;
  }
  glTexSubImage2D(target,0,0,
		  0,tx,ty,GL_LUMINANCE,
		  GL_UNSIGNED_BYTE,buf);

  glBegin (GL_TRIANGLE_FAN);
  glTexCoord2d (0, 0);
  glVertex2d (0, ty);
  glTexCoord2d (0, ty);
  glVertex2d (0, 0);
  glTexCoord2d (tx, ty);
  glVertex2d (tx, 0);
  glTexCoord2d (tx, 0);
  glVertex2d (tx,ty);
  glEnd ();
  
  glutSwapBuffers();
  glutPostRedisplay();
}

void
key(unsigned char key, int x, int y)
{
        switch(key){
        case 27:
        case 'q':
                exit(0);
        }
}


void
initgl()
{
  int argc=0;
  glutInit(&argc,0);
  glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB);
  glutInitWindowSize(tx,ty);
  glutInitWindowPosition(0,0);
  glutCreateWindow("sensicam qe");

  glutDisplayFunc(draw);
  glutReshapeFunc(reshape);
  glutKeyboardFunc(key);

  unsigned int obj;
  glGenTextures(1,&obj);
  glEnable(target);
  glBindTexture(target,obj);
  glTexParameteri(target,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(target,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexImage2D(target,0,GL_LUMINANCE,tx,ty,0,
	       GL_LUMINANCE,GL_UNSIGNED_BYTE,0);
}

int
main(int argc, char **argv)
{
  initgl();
  
  int // hdriver,
    board=0;
  int r=sen_initboard(board,&hdriver);
  assert(r>=0);
  r=sen_setup_camera(hdriver);
  if(r<0)
    sen_closeboard(&hdriver);

  // struct cam_param params;
  sen_get_cam_param(hdriver,&params);
  printf("camtype: %d\n",params.cam_typ);
  printf("ccdsize: %dx%d\n",params.ccdwidth,params.ccdheight);
  assert(params.cam_typ==FASTEXPQE);
  
  glutMainLoop();

}
