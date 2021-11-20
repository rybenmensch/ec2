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
#include "ext.h"
#include "ext_obex.h"
#include "ext_common.h"
#include "ext_buffer.h"
#include "z_dsp.h"
#include "gen.h"
#include "mydsp.h"

enum types{INTERNAL=0, EXTERNAL, EXTERNAL_INTERP};

typedef struct _voice{
    t_bool is_active;
    t_sample playback_rate;
    t_sample play_phase;
    t_sample window_phase;
    t_sample window_increment;
    t_sample envelope_shape;
    t_sample pan;
    t_sample amplitude;    
    t_sample scan_begin;
    t_sample scan_end;
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
    
    t_sample *tukey;
    t_sample *expodec;
    t_sample *rexpodec;
    t_atom_long window_size;
    
    t_buffer_ref *window_ext_ref;
    t_buffer_obj *window_ext_obj;
    t_sample *window_ext_samps;
    t_buffer_ref *window_ext_2_ref;
    t_buffer_obj *window_ext_2_obj;
    t_sample *window_ext_2_samps;
    
    t_ec2_buffer *windowbuffers;
    int window_type;
    
    int scan_type;
    t_atom_long total_voices;
    t_atom_long active_voices;
    t_voice *voices;
    
    t_bool init;
    t_sample scan_count;
    
    t_sample norm;
    
    short *count;
} t_ec2;

t_symbol *ps_buffer_modified;
t_class *ec2_class;

t_atom_long inlet_amount;   //why the fuck is this a global ??

void *ec2_new(t_symbol *s,  long argc, t_atom *argv);
void ec2_free(t_ec2 *x);
void ec2_assist(t_ec2 *x, void *b, long m, long a, char *s);

void ec2_perform64(t_ec2 *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void ec2_perform64_noscan(t_ec2 *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);

void ec2_scan_type(t_ec2 *x, t_symbol *s){
    if(s==gensym("internal")){
        x->scan_type = INTERNAL;
    }else if(s==gensym("external")){
        x->scan_type = EXTERNAL;
    }
}

void ec2_window_type(t_ec2 *x, t_symbol *s){
    if(s==gensym("internal")){
        x->window_type = INTERNAL;
    }else if(s==gensym("external")){
        if(!x->window_ext_ref){
            object_error((t_object *)x, "No external window buffer set yet!");
            return;
        }
        x->window_type = EXTERNAL;
    }
}

void ec2_normalization(t_ec2 *x, t_atom_long a){
    a = CLAMP(a, 0, 1);
    x->norm = (a)?(t_sample)1./x->total_voices:1;
}

void ec2_window_ext_calc(t_ec2 *x){
    t_atom_long framecount  = buffer_getframecount(x->window_ext_obj);
    t_atom_long chans       = buffer_getchannelcount(x->window_ext_obj);
    t_float *buffersamps    = buffer_locksamples(x->window_ext_obj);

    if(x->window_ext_samps){
        sysmem_freeptr(x->window_ext_samps);
    }
    
    x->window_ext_samps = (t_sample *)sysmem_newptr(x->window_size*sizeof(t_sample));
    
    if(framecount==512){
        for(long i=0;i<x->window_size;i++){
            long index = chans*i;
            x->window_ext_samps[i] = (t_sample)buffersamps[index];
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
            x->window_ext_samps[i] = samp;
            index_accum += factor;
        }
    }
    buffer_unlocksamples(x->window_ext_obj);
}

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

void ec2_do_window_ext(t_ec2 *x, t_symbol *s, long ac, t_atom *av){
    if(ac<=0 || ac>2){
        object_error((t_object *)x, "Wrong arguments to windowext");
        object_error((t_object *)x, "Correct format is: windowext buffername");
        object_error((t_object *)x, "or windowext buffername1 buffername2");
        return;
    }

    t_symbol *name1 = atom_getsym(av);
    t_bool return_1 = check_buffer(x, &(x->window_ext_ref), &(x->window_ext_obj), name1);
    
    //nested if's of death..
    //not super proud, rethink LATER(tm)
    if(return_1){
        if(ac==1){
            ec2_window_ext_calc(x);
        }else{
            t_symbol *name2 = atom_getsym(av+1);
            t_bool return_2 = check_buffer(x, &(x->window_ext_2_ref), &(x->window_ext_2_obj), name2);
            
            if(return_2){
                ec2_window_ext_calc(x);
            }else{
                object_error((t_object *)x, "Buffer %s probably doesn't exist.", name2->s_name);
                return;
            }
        }
    }else{
        object_error((t_object *)x, "Buffer %s probably doesn't exist.", name1->s_name);
        return;
    }
}

void ec2_window_ext(t_ec2 *x, t_symbol *s, long ac, t_atom *av){
    defer_low(x, (method)ec2_do_window_ext, s, ac, av);
}

void ec2_buffer_limits(t_ec2 *x){
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

void ec2_set(t_ec2 *x, t_symbol *s, long ac, t_atom *av){
    defer_low(x, (method)ec2_doset, s, ac, av);
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

void ec2_dsp64(t_ec2 *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags){
    x->samplerate = sys_getsr();
    sysmem_copyptr(count, x->count, inlet_amount*sizeof(short));
    
    //handle different scan types in one perform method? because else dsp must be recompiled to switch
    //dynamically between internal and external (LAME)
    //we could reuse the function pointer trick to calculate the scan position
    //and reassign ptr at block rate (as in window type internal/external)
    
    if(count[inlet_amount-1]){
        x->scan_type = EXTERNAL;
        object_method(dsp64, gensym("dsp_add64"), x, ec2_perform64_noscan, 0, NULL);
    }else{
        x->scan_type = INTERNAL;
        object_method(dsp64, gensym("dsp_add64"), x, ec2_perform64, 0, NULL);
    }
}

long ec2_multichanneloutputs(t_ec2 *x, long index){
    if(3==index){
        return 3;
    }if(2==index){
        return x->total_voices;
    }else{
        return 1;
    }
}

void calculate_windows(t_ec2 *x){
    //weibull for (r)expodec?
    t_atom_long size = x->window_size-1;
    
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
        x->tukey[i] = val;
    }
    
    //CALCULATE (R)EXPODEC
    for(int i=0;i<size+1;i++){
        t_sample phase = (float)i/size;
        //experimental value
        t_sample a = 36;
        t_sample val = (powf(a, phase)-1)/(a-1);
        x->expodec[size-i] = val;
        x->rexpodec[i] = val;
    }
}

#endif /* ec2__h */
