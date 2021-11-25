#ifndef window_h
#define window_h
#include "common.h"

typedef struct _window{
	t_atom_long size;
	t_sample *tukey;
	t_sample *expodec;
	t_sample *rexpodec;

	t_sample *window_ext_samps;
	t_buffer_ref *window_ext_ref;
	t_buffer_obj *window_ext_obj;

    t_sample *window_ext_2_samps;
    t_buffer_ref *window_ext_2_ref;
    t_buffer_obj *window_ext_2_obj;

	t_sample (*window)(struct _window*, struct _voice*);
}t_window;

t_sample window_internal(t_window *w, struct _voice *v);
t_sample window_direct(t_window *w, struct _voice *v);
t_sample window_external(t_window *w, struct _voice *v);

void calculate_windows(t_window *window);

void window_init(t_window *window, t_atom_long size){
	window->size     = size;
	window->tukey    = (t_sample *)sysmem_newptr(window->size*sizeof(t_sample));
	window->expodec  = (t_sample *)sysmem_newptr(window->size*sizeof(t_sample));
	window->rexpodec = (t_sample *)sysmem_newptr(window->size*sizeof(t_sample));

	window->window				= window_internal;
	//window->window				= window_direct;

	window->window_ext_samps 	= NULL;
	window->window_ext_ref		= NULL;
	window->window_ext_obj		= NULL;
	window->window_ext_2_samps 	= NULL;
	window->window_ext_2_ref	= NULL;
	window->window_ext_2_obj	= NULL;

	calculate_windows(window);
}

void window_free(t_window *window){
	object_free(window->window_ext_ref);
	object_free(window->window_ext_2_ref);

	if(window->tukey){
		sysmem_freeptr(window->tukey);
	}
	if(window->expodec){
		sysmem_freeptr(window->expodec);
	}
	if(window->rexpodec){
		sysmem_freeptr(window->rexpodec);
	}
	if(window->window_ext_samps){
		sysmem_freeptr(window->window_ext_samps);
	}
}

void calculate_windows(t_window *window){
	/*
	weibull expodec?
	phase between 0 and 1
	(1.6/0.26) * pow((phase/1.6), (1.6-1)) * exp(-1 * pow((phase/0.26), 1.6))
	*/
    t_atom_long size = window->size-1;

    //CALCULATE TUKEY
    t_sample alpha = 0.5;
    t_sample anm12 = 0.5*alpha*size;

    for(int i=0;i<size+1;i++){
        t_sample val = 0;
        if(i<=anm12){
            val = 0.5*(1+cos(PI*(i/anm12 - 1)));
        }else if(i<size*(1-0.5*alpha)){
            val = 1;
        }else{
            val = 0.5*(1+cosf(PI*(i/anm12 - 2/alpha + 1)));
        }
        window->tukey[i] = val;
    }

    //CALCULATE (R)EXPODEC
    for(int i=0;i<size+1;i++){
        t_sample phase = (float)i/size;
        //experimental value
        t_sample a = 36;
        t_sample val = (powf(a, phase)-1)/(a-1);
        window->expodec[size-i] = val;
        window->rexpodec[i] = val;
    }
}

#endif
