/*
EC2CLONE
Copyright (C) Manolo Müller, 2020
 
This file is part of EC2CLONE.

EC2CLONE is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

EC2CLONE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with EC2CLONE.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ec2__h
#define ec2__h
#include "common.h"
#include "gen.h"
#include "mydsp.h"
#include "scanner.h"
#include "window.h"

typedef struct _voice{
    t_bool   is_active;
	t_bool   is_done;
    t_sample playback_rate;
    t_sample play_phase;
    t_sample window_phase;
    t_sample window_increment;
    t_sample envelope_shape;
    t_sample pan;
    t_sample amplitude;    
    t_sample scan_begin;
    t_sample scan_end;
	//t_sample glisson[2];
}t_voice;

typedef struct _ec2_buffer{
    t_buffer_ref    *ref;
    t_buffer_obj    *obj;
    t_sample        *samples;
    t_atom_long     size;
    t_atom_long     nchans;
    t_bool          buffer_modified;
}t_ec2_buffer;

typedef struct _ec2 {
    t_pxobject p_ob;
    t_float samplerate;
    t_buffer_ref *buffer_reference;
    t_buffer_obj *buffer_obj;
    t_atom_long buffer_size;    //should this be the exact size or one less?
    t_sample *buffersamps;
    t_atom_long channel_count;
    t_bool buffer_modified;
    t_bool no_buffer;
    
    //t_sample *tukey;
    //t_sample *expodec;
    //t_sample *rexpodec;
    //t_atom_long window_size;
    //int window_type;
	t_window window;

	t_scanner scanner;

	t_atom_long total_voices;
    t_atom_long active_voices;
    t_voice *voices;
    
    t_bool init;
    t_sample scan_count;
    
    t_sample norm;

	t_sample glisson[2];
	t_sample glisson_inv[2];
	t_bool glisson_rand;

    short *count;
} t_ec2;

t_symbol *ps_buffer_modified;
t_class *ec2_class;

t_atom_long inlet_amount;

void *ec2_new(t_symbol *s,  long argc, t_atom *argv);
void ec2_free(t_ec2 *x);
void ec2_assist(t_ec2 *x, void *b, long m, long a, char *s);
void ec2_norm(t_ec2 *x, t_atom_long a);
void ec2_glisson(t_ec2 *x, t_symbol *s, long ac, t_atom *av);
void ec2_glissonr(t_ec2 *x, t_symbol *s, long ac, t_atom *av);
void ec2_scan_type(t_ec2 *x, t_symbol *s);
void ec2_window_type(t_ec2 *x, t_symbol *s);
void ec2_window_ext_calc(t_ec2 *x);
void ec2_buffer_copy(t_ec2 *x, t_ec2_buffer *b);
t_bool check_create_buffer(t_ec2 *x, t_ec2_buffer *b, t_symbol *name);
t_bool check_buffer(t_ec2 *x, t_buffer_ref **ref, t_buffer_obj **obj, t_symbol *name);
void ec2_do_window_ext(t_ec2 *x, t_symbol *s, long ac, t_atom *av);
void ec2_window_ext(t_ec2 *x, t_symbol *s, long ac, t_atom *av);
void ec2_buffer_limits(t_ec2 *x);
void ec2_doset(t_ec2 *x, t_symbol *s, long ac, t_atom *av);
void ec2_set(t_ec2 *x, t_symbol *s, long ac, t_atom *av);
void ec2_dblclick(t_ec2 *x);
t_max_err ec2_notify(t_ec2 *x, t_symbol *s, t_symbol *msg, void *sender, void *data);
void ec2_perform64(t_ec2 *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void ec2_dsp64(t_ec2 *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void ec2_norm(t_ec2 *x, t_atom_long a){
    a = CLAMP(a, 0, 1);
    x->norm = (a)?(t_sample)1./x->total_voices:1;
}

void ec2_glisson(t_ec2 *x, t_symbol *s, long ac, t_atom *av){
	//structure of message: glisson startoffset endoffset (in semitones)
    if(ac!=2){
        object_error((t_object *)x, "Wrong arguments to glisson");
        object_error((t_object *)x, "Correct format is: glisson (float)startoffset (float)endoffset");
        return;
    }
	t_sample gl[2];
	for(int i=0;i<2;i++){
		if(atom_gettype(av+i) == A_FLOAT){
			gl[i] = (t_sample)atom_getfloat(av+i);
		}else if(atom_gettype(av+i) == A_LONG){
			gl[i] = (t_sample)atom_getlong(av+i);
		}
	}
	//umwandeln in lineare funktion, die bei 0 startoffset, bei 1 endoffset ist
	t_sample b = gl[0];
	t_sample m = gl[1] - b;
	x->glisson[0] = m;
	x->glisson[1] = b;
}

void ec2_glissonr(t_ec2 *x, t_symbol *s, long ac, t_atom *av){
	//structure of message: glisson startoffset endoffset (in semitones)
    if(ac!=2){
        object_error((t_object *)x, "Wrong arguments to glissonr");
        object_error((t_object *)x, "Correct format is: glisson (float)startoffset (float)endoffset");
        return;
    }
	t_sample gl[2];
	for(int i=0;i<2;i++){
		if(atom_gettype(av+i) == A_FLOAT){
			gl[i] = (t_sample)atom_getfloat(av+i);
		}else if(atom_gettype(av+i) == A_LONG){
			gl[i] = (t_sample)atom_getlong(av+i);
		}
	}
	//umwandeln in lineare funktion, die bei 0 startoffset, bei 1 endoffset ist
	t_sample b = gl[0];
	t_sample m = gl[1] - b;
	x->glisson[0] = m;
	x->glisson[1] = b;
}

void ec2_scan_type(t_ec2 *x, t_symbol *s){
    if(s==gensym("internal")){
		x->scanner.type = INTERNAL;
    }else if(s==gensym("external")){
		x->scanner.type = EXTERNAL;
    }
}

void ec2_window_type(t_ec2 *x, t_symbol *s){
	t_window *w = &x->window;
    if(s==gensym("internal")){
		w->window = window_internal;
		//w->window = window_direct;
    }else if(s==gensym("external")){
        if(!w->window_ext_ref){
            object_error((t_object *)x, "No external window buffer set yet!");
            return;
        }
		w->window = window_external;
    }
}

void ec2_window_ext(t_ec2 *x, t_symbol *s, long ac, t_atom *av){
    defer_low(x, (method)ec2_do_window_ext, s, ac, av);
}

void ec2_do_window_ext(t_ec2 *x, t_symbol *s, long ac, t_atom *av){
	t_window *w = &x->window;
    if(ac<=0 || ac>2){
        object_error((t_object *)x, "Wrong arguments to windowext");
        object_error((t_object *)x, "Correct format is: windowext buffername");
        object_error((t_object *)x, "or windowext buffername1 buffername2");
        return;
    }

    t_symbol *name1 = atom_getsym(av);
    t_bool return_1 = check_buffer(x, &(w->window_ext_ref), &(w->window_ext_obj), name1);
    
    //nested if's of death..
    //not super proud, rethink LATER(tm)
    if(return_1){
        if(ac==1){
            ec2_window_ext_calc(x);
        }else{
			/*
            t_symbol *name2 = atom_getsym(av+1);
            t_bool return_2 = check_buffer(x, &(x->window_ext_2_ref), &(x->window_ext_2_obj), name2);
            
            if(return_2){
                ec2_window_ext_calc(x);
            }else{
                object_error((t_object *)x, "Buffer %s probably doesn't exist.", name2->s_name);
                return;
            }
			*/
        }
    }else{
        object_error((t_object *)x, "Buffer %s probably doesn't exist.", name1->s_name);
        return;
    }
}

void ec2_window_ext_calc(t_ec2 *x){
	t_window *w = &x->window;
    t_atom_long framecount  = buffer_getframecount(w->window_ext_obj);
    t_atom_long chans       = buffer_getchannelcount(w->window_ext_obj);
    t_float *buffersamps    = buffer_locksamples(w->window_ext_obj);

    if(w->window_ext_samps){
        sysmem_freeptr(w->window_ext_samps);
    }

    w->window_ext_samps = (t_sample *)sysmem_newptr(w->size*sizeof(t_sample));

    if(framecount==512){
        for(long i=0;i<w->size;i++){
            long index = chans*i;
			//crash on reinstantiating buffer object used as external buffer
            w->window_ext_samps[i] = (t_sample)buffersamps[index];
        }
    }else{
        t_sample temp_buffer[framecount];

        for(long i=0;i<framecount;i++){
            temp_buffer[i] = (t_sample)buffersamps[i];
        }
        //t_sample factor = (t_sample)x->window_size/(t_sample)framecount;
        t_sample factor = (t_sample)framecount/(t_sample)w->size;

        t_sample index_accum = 0;
        for(long i=0;i<w->size;i++){
            t_sample samp = peek(temp_buffer, framecount-1, index_accum);
            w->window_ext_samps[i] = samp;
            index_accum += factor;
        }
    }
    buffer_unlocksamples(w->window_ext_obj);
}

/*
void ec2_buffer_copy(t_ec2 *x, t_ec2_buffer *b){
    t_atom_long framecount  = buffer_getframecount(b->obj);
    t_atom_long chans       = buffer_getchannelcount(b->obj);
    t_float *buffersamps    = buffer_locksamples(b->obj);

    if(b->samples){
        sysmem_freeptr(b->samples);
    }

    b->samples = (t_sample *)sysmem_newptr(b->size*sizeof(t_sample));

    if(framecount==x->window_size){
        for(long i=0;i<x->window_size;i++){
            long index = chans*i;
            b->samples[i] = (t_sample)buffersamps[index];
        }
    }else{
        t_sample temp_buffer[framecount];

        for(long i=0;i<framecount;i++){
            temp_buffer[i] = (t_sample)buffersamps[i];
        }
        //t_sample factor = (t_sample)x->window_size/(t_sample)framecount;
        t_sample factor = (t_sample)framecount/(t_sample)x->window_size;

        t_sample index_accum = 0;
        for(long i=0;i<x->window_size;i++){
            t_sample samp = peek(temp_buffer, framecount-1, index_accum);
            b->samples[i] = samp;
            index_accum += factor;
        }
    }
    buffer_unlocksamples(b->obj);
}
 */

/*
t_bool check_create_buffer(t_ec2 *x, t_ec2_buffer *b, t_symbol *name){
    if(!b->ref){
        b->ref = buffer_ref_new((t_object *)x, name);
    }else{
        buffer_ref_set(b->ref, name);
    }

    if((b->obj = buffer_ref_getobject(b->ref))){
        return TRUE;
    }else{
        return FALSE;
    }
}
 */


t_bool check_buffer(t_ec2 *x, t_buffer_ref **ref, t_buffer_obj **obj, t_symbol *name){
    if(!(*ref)){
        (*ref) = buffer_ref_new((t_object *)x, name);
    }else{
        buffer_ref_set((*ref), name);
    }

    if(((*obj) = buffer_ref_getobject(*ref))){
        return TRUE;
    }else{
        return FALSE;
    }
}

void ec2_buffer_limits(t_ec2 *x){
	//maybe at one point we should stop copying the buffer
	//and just use the pointer ?
    x->buffer_size      = buffer_getframecount(x->buffer_obj)-1;
    x->channel_count    = buffer_getchannelcount(x->buffer_obj);
    
    if(x->buffersamps){
        sysmem_freeptr(x->buffersamps);
    }
    
    x->buffersamps = (t_sample *)sysmem_newptr((x->buffer_size+1) * sizeof(t_sample));
    t_float *buffersamps = buffer_locksamples(x->buffer_obj);
    
    if(!buffersamps){
        post("couldn't lock samples");
        return;
    }
    
    for(long i=0;i<=x->buffer_size;i++){
        long index = x->channel_count*i;
        x->buffersamps[i] = (t_sample)buffersamps[index];
    }
    
    buffer_unlocksamples(x->buffer_obj);
}

void ec2_set(t_ec2 *x, t_symbol *s, long ac, t_atom *av){
    defer_low(x, (method)ec2_doset, s, ac, av);
}

void ec2_doset(t_ec2 *x, t_symbol *s, long ac, t_atom *av){
    t_symbol *name;
    name = (ac)?atom_getsym(av):gensym("");
    
    if(!x->buffer_reference){
        x->buffer_reference = buffer_ref_new((t_object *)x, name);
    }else{
        buffer_ref_set(x->buffer_reference, name);
    }
    
    if((x->buffer_obj = buffer_ref_getobject(x->buffer_reference))){
        x->no_buffer = FALSE;
        ec2_buffer_limits(x);
    }else{
        error("Buffer %s probably doesn't exist.", name->s_name);
        x->no_buffer = TRUE;
    }
    
    /*
    t_bool retval = check_buffer(x, &(x->buffer_reference), &(x->buffer_obj), s);
    if(retval){
        x->no_buffer = FALSE;
        ec2_buffer_limits(x);
    }else{
        error("Buffer %s probably doesn't exist.", name->s_name);
        x->no_buffer = TRUE;
    }
    */
}

void ec2_dblclick(t_ec2 *x){
    buffer_view(x->buffer_obj);
}

t_max_err ec2_notify(t_ec2 *x, t_symbol *s, t_symbol *msg, void *sender, void *data){
    /* DIAGNOSTICS:
    t_symbol *namespace = NULL, *name = NULL;
    object_findregisteredbyptr(&namespace, &name, sender);
    if(namespace&&name){
        post("namespace: %s\tname: %s", namespace->s_name, name->s_name);
    }
    */
    
    if(msg==ps_buffer_modified){
        //post("notification\ts: %s\tmsg: %s\t", s->s_name, msg->s_name);
        x->buffer_modified = TRUE;
        
        //here we should check which buffer sent the information
        ec2_window_ext_calc(x);
    }
    
    return buffer_ref_notify(x->buffer_reference, s, msg, sender, data);
}

long ec2_mcout(t_ec2 *x, long index){
    if(3==index){
        return 3;
    }if(2==index){
        return x->total_voices;
    }else{
        return 1;
    }
}

void ec2_assist(t_ec2 *x, void *b, long m, long a, char *s){
    if(m == ASSIST_INLET){
        switch(a){
            case 0:
                sprintf(s, "(signal) Trigger");
                break;
            case 1:
                sprintf(s, "(signal) Playback rate");
                break;
            case 2:
                sprintf(s, "(signal) Scan begin (0. - 1.)");
                break;
            case 3:
                sprintf(s, "(signal) Scan range (0. - 1.)");
                break;
            case 4:
                sprintf(s, "(signal) Scan speed");
                break;
            case 5:
                sprintf(s, "(signal) Grain duration (ms)");
                break;
            case 6:
                sprintf(s, "(signal) Envelope shape (0. - 1.)");
                break;
            case 7:
                sprintf(s, "(signal) Pan (-1. - 1.)");
                break;
            case 8:
                sprintf(s, "(signal) Amplitude (0. - 1.)");
                break;
            case 9:
                sprintf(s, "(signal) External scan position (0. - 1.)");
        }
    }else{
        switch(a){
            case 0:
                sprintf(s, "(signal) Left output");
                break;
            case 1:
                sprintf(s, "(signal) Right output");
                break;
            case 2:
                sprintf(s, "(mcsignal) Busymap");
                break;
            case 3:
                sprintf(s, "(mcsignal) Scanhead and range");
                break;
        }
    }
}

#endif /* ec2__h */
