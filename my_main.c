/*========== my_main.c ==========

  This is the only file you need to modify in order
  to get a working mdl project (for now).

  my_main.c will serve as the interpreter for mdl.
  When an mdl script goes through a lexer and parser,
  the resulting operations will be in the array op[].

  Your job is to go through each entry in op and perform
  the required action from the list below:

  push: push a new origin matrix onto the origin stack
  pop: remove the top matrix on the origin stack

  move/scale/rotate: create a transformation matrix
                     based on the provided values, then
                     multiply the current top of the
                     origins stack by it.

  box/sphere/torus: create a solid object based on the
                    provided values. Store that in a
                    temporary matrix, multiply it by the
                    current top of the origins stack, then
                    call draw_polygons.

  line: create a line based on the provided values. Stores
        that in a temporary matrix, multiply it by the
        current top of the origins stack, then call draw_lines.

  save: call save_extension with the provided filename

  display: view the image live
  =========================*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "parser.h"
#include "symtab.h"
#include "y.tab.h"

#include "matrix.h"
#include "ml6.h"
#include "display.h"
#include "draw.h"
#include "stack.h"
#include "gmath.h"


/*======== void first_pass() ==========
  Inputs:
  Returns:

  Checks the op array for any animation commands
  (frames, basename, vary)

  Should set num_frames and basename if the frames
  or basename commands are present

  If vary is found, but frames is not, the entire
  program should exit.

  If frames is found, but basename is not, set name
  to some default value, and print out a message
  with the name being used.
  ====================*/
int num_frames;
char name[128];
void first_pass() {
  //in order to use name and num_frames throughout
  //they must be extern variables
  //extern int num_frames;
  //extern char name[128];
  num_frames = 1;
  
  int var;
  var = 0;

  int i;
  for(i = 0; i < lastop; i++){

    if(op[i].opcode == FRAMES){
      num_frames = op[i].op.frames.num_frames;
    }
    else if(op[i].opcode == BASENAME){
      strcpy(name, op[i].op.basename.p->name);
    }
    else if(op[i].opcode == VARY && var == 0){
      var = 1;
    }
  }

}

/*======== struct vary_node ** second_pass() ==========
  Inputs:
  Returns: An array of vary_node linked lists

  In order to set the knobs for animation, we need to keep
  a seaprate value for each knob for each frame. We can do
  this by using an array of linked lists. Each array index
  will correspond to a frame (eg. knobs[0] would be the first
  frame, knobs[2] would be the 3rd frame and so on).

  Each index should contain a linked list of vary_nodes, each
  node contains a knob name, a value, and a pointer to the
  next node.

  Go through the opcode array, and when you find vary, go
  from knobs[0] to knobs[frames-1] and add (or modify) the
  vary_node corresponding to the given knob with the
  appropirate value.
  ====================*/
struct vary_node ** second_pass() {
  int start;
  int end;
  double sval;
  double eval;
  double d, absd;
  char name[128];
  struct vary_node ** knobs = malloc(num_frames * sizeof(struct vary_node));
  int i;
	double expmod = 0;
  
  for(i = 0; i < lastop; i++){
    if(op[i].opcode == VARY){
      start = op[i].op.vary.start_frame;
      end = op[i].op.vary.end_frame;
      sval = op[i].op.vary.start_val;
      eval = op[i].op.vary.end_val;
			expmod = op[i].op.vary.exponent;
			
      strcpy(name, op[i].op.vary.p->name);
      d = ((eval - sval) / (end - start));
			absd = fabs(d);
      int j;
			
			if(expmod){
	      for(j = start; j <= end; j++){
	        struct vary_node * now = malloc(sizeof(struct vary_node));
	        strcpy(now->name, name);
	        now->value = sval + d * (j - start) * (pow((absd * (j - start)), (expmod - 1)));
	        now->next = knobs[j];
	        knobs[j] = now;
	      }
			}
			if(expmod == 0){
      	for(j = start; j <= end; j++){
					struct vary_node * now = malloc(sizeof(struct vary_node));
      		strcpy(now->name, name);
      		now->value = sval + d * (j - start);
      		now->next = knobs[j];
      		knobs[j] = now;
      	}
			}
    }
  }
  //print_knobs();
  //print_knobs();

  return knobs;
}

/*======== void print_knobs() ==========
Inputs:
Returns:

Goes through symtab and display all the knobs and their
currnt values
====================*/
void print_knobs() {
  int i;

  printf( "ID\tNAME\t\tTYPE\t\tVALUE\n" );
  for ( i=0; i < lastsym; i++ ) {

    if ( symtab[i].type == SYM_VALUE ) {
      printf( "%d\t%s\t\t", i, symtab[i].name );

      printf( "SYM_VALUE\t");
      printf( "%6.2f\n", symtab[i].s.value);
    }
  }
}

/*======== void my_main() ==========
  Inputs:
  Returns:

  This is the main engine of the interpreter, it should
  handle most of the commadns in mdl.

  If frames is not present in the source (and therefore
  num_frames is 1, then process_knobs should be called.

  If frames is present, the enitre op array must be
  applied frames time. At the end of each frame iteration
  save the current screen to a file named the
  provided basename plus a numeric string such that the
  files will be listed in order, then clear the screen and
  reset any other data structures that need it.

  Important note: you cannot just name your files in
  regular sequence, like pic0, pic1, pic2, pic3... if that
  is done, then pic1, pic10, pic11... will come before pic2
  and so on. In order to keep things clear, add leading 0s
  to the numeric portion of the name. If you use sprintf,
  you can use "%0xd" for this purpose. It will add at most
  x 0s in front of a number, if needed, so if used correctly,
  and x = 4, you would get numbers like 0001, 0002, 0011,
  0487
  ====================*/
void my_main() {

  int i;
  struct matrix *tmp;
  struct stack *systems;
  screen t;
  zbuffer zb;
  color g;
  g.red = 0;
  g.green = 0;
  g.blue = 0;
  double step_3d = 100;
  double theta;
  double knob_value, xval, yval, zval;

  //Lighting values here for easy access
  color ambient;
  double light[2][3];
  double view[3];
  double areflect[3];
  double dreflect[3];
  double sreflect[3];

  ambient.red = 100;
  ambient.green = 0;
  ambient.blue = 255;

  light[LOCATION][0] = 0.5;
  light[LOCATION][1] = 0.75;
  light[LOCATION][2] = 1;

  light[COLOR][RED] = 150;
  light[COLOR][GREEN] = 0;
  light[COLOR][BLUE] = 255;

  view[0] = 0;
  view[1] = 0;
  view[2] = 1;

  areflect[RED] = 0.1;
  areflect[GREEN] = 0.1;
  areflect[BLUE] = 0.1;

  dreflect[RED] = 0.5;
  dreflect[GREEN] = 0.5;
  dreflect[BLUE] = 0.5;

  sreflect[RED] = 0.5;
  sreflect[GREEN] = 0.5;
  sreflect[BLUE] = 0.5;

  systems = new_stack();
  tmp = new_matrix(4, 1000);
  clear_screen( t );
  clear_zbuffer(zb);

  first_pass();

 if(num_frames > 1){
    struct vary_node ** knobs = second_pass();
    int framec;

    for(framec = 0; framec < num_frames; framec++){
      struct vary_node * now = knobs[framec];
      
      while(now){
        if (!lookup_symbol(now->name))
        	add_symbol(now->name, SYM_VALUE, &(now->value));
      else{
	      set_value(lookup_symbol(now->name),now->value);
        now=now->next;
      }
    }
    printf("\n[frame %d] printing knobs \n", framec);
    print_knobs();
  for (i=0;i<lastop;i++) {
    //printf("%d: ",i);
    switch (op[i].opcode)
      {
      case SPHERE:

        if (op[i].op.sphere.constants != NULL)
          {
            //printf("\tconstants: %s",op[i].op.sphere.constants->name);
          }
        if (op[i].op.sphere.cs != NULL)
          {
            //printf("\tcs: %s",op[i].op.sphere.cs->name);
          }
        add_sphere(tmp, op[i].op.sphere.d[0],
                   op[i].op.sphere.d[1],
                   op[i].op.sphere.d[2],
                   op[i].op.sphere.r, step_3d);
        matrix_mult( peek(systems), tmp );
        draw_polygons(tmp, t, zb, view, light, ambient,
                      areflect, dreflect, sreflect);
        tmp->lastcol = 0;
        break;
      case TORUS:

        if (op[i].op.torus.constants != NULL)
          {
            //printf("\tconstants: %s",op[i].op.torus.constants->name);
          }
        if (op[i].op.torus.cs != NULL)
          {
            //printf("\tcs: %s",op[i].op.torus.cs->name);
          }
        add_torus(tmp,
                  op[i].op.torus.d[0],
                  op[i].op.torus.d[1],
                  op[i].op.torus.d[2],
                  op[i].op.torus.r0,op[i].op.torus.r1, step_3d);
        matrix_mult( peek(systems), tmp );
        draw_polygons(tmp, t, zb, view, light, ambient,
                      areflect, dreflect, sreflect);
        tmp->lastcol = 0;
        break;
      case BOX:
 
        if (op[i].op.box.constants != NULL)
          {
            //printf("\tconstants: %s",op[i].op.box.constants->name);
          }
        if (op[i].op.box.cs != NULL)
          {
            //printf("\tcs: %s",op[i].op.box.cs->name);
          }
        add_box(tmp,
                op[i].op.box.d0[0],op[i].op.box.d0[1],
                op[i].op.box.d0[2],
                op[i].op.box.d1[0],op[i].op.box.d1[1],
                op[i].op.box.d1[2]);
        matrix_mult( peek(systems), tmp );
        draw_polygons(tmp, t, zb, view, light, ambient,
                      areflect, dreflect, sreflect);
        tmp->lastcol = 0;
        break;
	      case PRISM:
 
	        if (op[i].op.prism.constants != NULL)
	          {
	            //printf("\tconstants: %s",op[i].op.box.constants->name);
	          }
	        if (op[i].op.prism.cs != NULL)
	          {
	            //printf("\tcs: %s",op[i].op.box.cs->name);
	          }
	        add_prism(tmp,
	                op[i].op.prism.d0[0],op[i].op.prism.d0[1],
	                op[i].op.prism.d0[2],
	                op[i].op.prism.d1[0],op[i].op.prism.d1[1],
	                op[i].op.prism.d1[2],
	                op[i].op.prism.d2[0],op[i].op.prism.d2[1],
	                op[i].op.prism.d2[2],
									op[i].op.prism.h);
	        matrix_mult( peek(systems), tmp );
	        draw_polygons(tmp, t, zb, view, light, ambient,
	                      areflect, dreflect, sreflect);
	        tmp->lastcol = 0;
	        break;
				
	      case CYLINDER:

	        if (op[i].op.cylinder.constants != NULL)
	          {
	            //printf("\tconstants: %s",op[i].op.cylinder.constants->name);
	          }
	        if (op[i].op.cylinder.cs != NULL)
	          {
	            //printf("\tcs: %s",op[i].op.cylinder.cs->name);
	          }
	        add_cylinder(tmp,
	                  op[i].op.cylinder.d[0],
	                  op[i].op.cylinder.d[1],
	                  op[i].op.cylinder.d[2],
	                  op[i].op.cylinder.r,op[i].op.cylinder.h, step_3d);
	        matrix_mult( peek(systems), tmp );
	        draw_polygons(tmp, t, zb, view, light, ambient,
	                      areflect, dreflect, sreflect);
	        tmp->lastcol = 0;
	        break;
				
		      case CONE:

		        if (op[i].op.cone.constants != NULL)
		          {
		            //printf("\tconstants: %s",op[i].op.cylinder.constants->name);
		          }
		        if (op[i].op.cone.cs != NULL)
		          {
		            //printf("\tcs: %s",op[i].op.cylinder.cs->name);
		          }
		        add_cone(tmp,
		                  op[i].op.cone.d[0],
		                  op[i].op.cone.d[1],
		                  op[i].op.cone.d[2],
		                  op[i].op.cone.r,op[i].op.cone.h, step_3d);
		        matrix_mult( peek(systems), tmp );
		        draw_polygons(tmp, t, zb, view, light, ambient,
		                      areflect, dreflect, sreflect);
		        tmp->lastcol = 0;
		        break;
      case LINE:

        if (op[i].op.line.constants != NULL)
          {
            //printf("\n\tConstants: %s",op[i].op.line.constants->name);
          }
        if (op[i].op.line.cs0 != NULL)
          {
            //printf("\n\tCS0: %s",op[i].op.line.cs0->name);
          }
        if (op[i].op.line.cs1 != NULL)
          {
            //printf("\n\tCS1: %s",op[i].op.line.cs1->name);
          }
        add_edge(tmp,
                 op[i].op.line.p0[0],op[i].op.line.p0[1],
                 op[i].op.line.p0[2],
                 op[i].op.line.p1[0],op[i].op.line.p1[1],
                 op[i].op.line.p1[2]);
        matrix_mult( peek(systems), tmp );
        draw_lines(tmp, t, zb, g);
        tmp->lastcol = 0;
        break;
      case MOVE:
        xval = op[i].op.move.d[0];
        yval = op[i].op.move.d[1];
        zval = op[i].op.move.d[2];
        printf("Move: %6.2f %6.2f %6.2f",
               xval, yval, zval);
        if (op[i].op.move.p != NULL)
          {
            printf("\tknob: %s",op[i].op.move.p->name);
   	    xval*= lookup_symbol(op[i].op.move.p->name)->s.value;
	    yval*= lookup_symbol(op[i].op.move.p->name)->s.value;
	    zval*= lookup_symbol(op[i].op.move.p->name)->s.value;
          }
        tmp = make_translate( xval, yval, zval );
        matrix_mult(peek(systems), tmp);
        copy_matrix(tmp, peek(systems));
        tmp->lastcol = 0;
        break;
      case SCALE:
        xval = op[i].op.scale.d[0];
        yval = op[i].op.scale.d[1];
        zval = op[i].op.scale.d[2];
        printf("Scale: %6.2f %6.2f %6.2f",
               xval, yval, zval);
        if (op[i].op.scale.p != NULL)
          {
            printf("\tknob: %s",op[i].op.scale.p->name);
	      xval*= lookup_symbol(op[i].op.scale.p->name)->s.value;
	      yval*= lookup_symbol(op[i].op.scale.p->name)->s.value;
	      zval*= lookup_symbol(op[i].op.scale.p->name)->s.value;
          }
        tmp = make_scale( xval, yval, zval );
        matrix_mult(peek(systems), tmp);
        copy_matrix(tmp, peek(systems));
        tmp->lastcol = 0;
        break;
      case ROTATE:
        xval = op[i].op.rotate.axis;
        theta = op[i].op.rotate.degrees;
        printf("Rotate: axis: %6.2f degrees: %6.2f",
               xval, theta);
        if (op[i].op.rotate.p != NULL)
          {
            printf("\tknob: %s",op[i].op.rotate.p->name);
	    theta*= lookup_symbol(op[i].op.rotate.p->name)->s.value;
          }
        theta*= (M_PI / 180);
        if (op[i].op.rotate.axis == 0 )
          tmp = make_rotX( theta );
        else if (op[i].op.rotate.axis == 1 )
          tmp = make_rotY( theta );
        else
          tmp = make_rotZ( theta );

        matrix_mult(peek(systems), tmp);
        copy_matrix(tmp, peek(systems));
        tmp->lastcol = 0;
        break;
      case PUSH:
        //printf("Push");
        push(systems);
        break;
      case POP:
        //printf("Pop");
        pop(systems);
        break;
      case SAVE:
        //printf("Save: %s",op[i].op.save.p->name);
        save_extension(t, op[i].op.save.p->name);
        break;
      case DISPLAY:
        //printf("Display");
        display(t);
        break;
      } //end opcode switch
    printf("\n");
  }//end operation loop

    char name2[12+strlen(name)];

    sprintf(name2, "anim/%s%03d.png", name, framec);
    //display(t);
    
    save_extension(t,name2);
    clear_screen(t);
    clear_zbuffer(zb);
    systems=new_stack();
    tmp=new_matrix(4,1000);
  }//end for loop for all frames
  make_animation(name);
 }
  else{

printf("num frames = 1\n");
    for (i=0;i<lastop;i++) {
      //printf("%d: ",i);
      switch (op[i].opcode)
        {
        case SPHERE:
   
          if (op[i].op.sphere.constants != NULL)
            {
              //printf("\tconstants: %s",op[i].op.sphere.constants->name);
            }
          if (op[i].op.sphere.cs != NULL)
            {
              //printf("\tcs: %s",op[i].op.sphere.cs->name);
            }
          add_sphere(tmp, op[i].op.sphere.d[0],
                     op[i].op.sphere.d[1],
                     op[i].op.sphere.d[2],
                     op[i].op.sphere.r, step_3d);
          matrix_mult( peek(systems), tmp );
          draw_polygons(tmp, t, zb, view, light, ambient,
                        areflect, dreflect, sreflect);
          tmp->lastcol = 0;
          break;
        case TORUS:
 
          if (op[i].op.torus.constants != NULL)
            {
              //printf("\tconstants: %s",op[i].op.torus.constants->name);
            }
          if (op[i].op.torus.cs != NULL)
            {
              //printf("\tcs: %s",op[i].op.torus.cs->name);
            }
          add_torus(tmp,
                    op[i].op.torus.d[0],
                    op[i].op.torus.d[1],
                    op[i].op.torus.d[2],
                    op[i].op.torus.r0,op[i].op.torus.r1, step_3d);
          matrix_mult( peek(systems), tmp );
          draw_polygons(tmp, t, zb, view, light, ambient,
                        areflect, dreflect, sreflect);
          tmp->lastcol = 0;
          break;
        case BOX:

          if (op[i].op.box.constants != NULL)
            {
              //printf("\tconstants: %s",op[i].op.box.constants->name);
            }
          if (op[i].op.box.cs != NULL)
            {
              //printf("\tcs: %s",op[i].op.box.cs->name);
            }
          add_box(tmp,
                  op[i].op.box.d0[0],op[i].op.box.d0[1],
                  op[i].op.box.d0[2],
                  op[i].op.box.d1[0],op[i].op.box.d1[1],
                  op[i].op.box.d1[2]);
          matrix_mult( peek(systems), tmp );
          draw_polygons(tmp, t, zb, view, light, ambient,
                        areflect, dreflect, sreflect);
          tmp->lastcol = 0;
          break;
        case LINE:
  
          if (op[i].op.line.constants != NULL)
            {
              //printf("\n\tConstants: %s",op[i].op.line.constants->name);
            }
          if (op[i].op.line.cs0 != NULL)
            {
              //printf("\n\tCS0: %s",op[i].op.line.cs0->name);
            }
          if (op[i].op.line.cs1 != NULL)
            {
              //printf("\n\tCS1: %s",op[i].op.line.cs1->name);
            }
          add_edge(tmp,
                   op[i].op.line.p0[0],op[i].op.line.p0[1],
                   op[i].op.line.p0[2],
                   op[i].op.line.p1[0],op[i].op.line.p1[1],
                   op[i].op.line.p1[2]);
          matrix_mult( peek(systems), tmp );
          draw_lines(tmp, t, zb, g);
          tmp->lastcol = 0;
          break;
        case MOVE:
          xval = op[i].op.move.d[0];
          yval = op[i].op.move.d[1];
          zval = op[i].op.move.d[2];
          printf("Move: %6.2f %6.2f %6.2f",
                 xval, yval, zval);
          if (op[i].op.move.p != NULL)
            {
              printf("\tknob: %s",op[i].op.move.p->name);
            }
          tmp = make_translate( xval, yval, zval );
          matrix_mult(peek(systems), tmp);
          copy_matrix(tmp, peek(systems));
          tmp->lastcol = 0;
          break;
        case SCALE:
          xval = op[i].op.scale.d[0];
          yval = op[i].op.scale.d[1];
          zval = op[i].op.scale.d[2];
          printf("Scale: %6.2f %6.2f %6.2f",
                 xval, yval, zval);
          if (op[i].op.scale.p != NULL)
            {
              printf("\tknob: %s",op[i].op.scale.p->name);
            }
          tmp = make_scale( xval, yval, zval );
          matrix_mult(peek(systems), tmp);
          copy_matrix(tmp, peek(systems));
          tmp->lastcol = 0;
          break;
        case ROTATE:
          xval = op[i].op.rotate.axis;
          theta = op[i].op.rotate.degrees;
          printf("Rotate: axis: %6.2f degrees: %6.2f",
                 xval, theta);
          if (op[i].op.rotate.p != NULL)
            {
              printf("\tknob: %s",op[i].op.rotate.p->name);
            }
          theta*= (M_PI / 180);
          if (op[i].op.rotate.axis == 0 )
            tmp = make_rotX( theta );
          else if (op[i].op.rotate.axis == 1 )
            tmp = make_rotY( theta );
          else
            tmp = make_rotZ( theta );

          matrix_mult(peek(systems), tmp);
          copy_matrix(tmp, peek(systems));
          tmp->lastcol = 0;
          break;
        case PUSH:
          //printf("Push");
          push(systems);
          break;
        case POP:
          //printf("Pop");
          pop(systems);
          break;
        case SAVE:
          //printf("Save: %s",op[i].op.save.p->name);
          save_extension(t, op[i].op.save.p->name);
          break;
        case DISPLAY:
          //printf("Display");
          display(t);
          break;
        } //end opcode switch
      printf("\n");
    }//end operation loop
}
 
}
