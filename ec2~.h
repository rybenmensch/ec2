#ifndef ec2__h
#define ec2__h
#include "ext.h"
#include "ext_obex.h"
#include "ext_common.h"
#include "ext_buffer.h"
#include "z_dsp.h"
#include "gen.h"
#include "mydsp.h"

enum windowtype{INTERNAL, EXTERNAL};

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

typedef struct _stream{
    t_bool is_active;   //potentially unneeded
    t_atom_long active_voices;
    t_voice *voices;
}t_stream;

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
    
    t_buffer_ref *window_ext_ref;
    t_buffer_obj *window_ext_obj;
    t_sample *window_ext_samps;
    //enum windowtype wt;
    int window_type;
    
    t_sample *tukey;
    t_sample *expodec;
    t_sample *rexpodec;
    t_atom_long window_size;
    
    t_atom_long total_voices;
    t_atom_long active_voices;
    t_voice *voices;
    
    t_bool init;
    t_sample scan_count;
    t_atom_long testcounter;
    
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
    
    x->window_type = EXTERNAL;
    buffer_unlocksamples(x->window_ext_obj);
}

void ec2_do_window_ext(t_ec2 *x, t_symbol *s, long ac, t_atom *av){
    t_symbol *name;
    name = (ac)?atom_getsym(av):gensym("");
    if(!x->window_ext_ref){
        x->window_ext_ref = buffer_ref_new((t_object *)x, name);
    }else{
        buffer_ref_set(x->window_ext_ref, name);
    }
    
    if((x->window_ext_obj = buffer_ref_getobject(x->window_ext_ref))==NULL){
        error("Buffer %s probably doesn't exist.", name->s_name);
        return;
    }else{
        ec2_window_ext_calc(x);
    }
}

void ec2_window_ext(t_ec2 *x, t_symbol *s, long ac, t_atom *av){
    defer_low(x, (method)ec2_do_window_ext, s, ac, av);
}

void ec2_buffer_limits(t_ec2 *x){
    //get dimensions etc here so that we don't have to do that in the perform routine
    if(x->buffer_obj){
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
            //just skip according to channel_count
            long index = x->channel_count*i;
            x->buffersamps[i] = (t_sample)buffersamps[index];
        }
        
        buffer_unlocksamples(x->buffer_obj);
    }else{
        post("can\'t get buffer reference");
    }
}

void ec2_doset(t_ec2 *x, t_symbol *s, long ac, t_atom *av){
    t_symbol *name;
    name = (ac)?atom_getsym(av):gensym("");
    
    if(!x->buffer_reference){
        x->buffer_reference = buffer_ref_new((t_object *)x, name);
    }else{
        buffer_ref_set(x->buffer_reference, name);
    }
    
    if((x->buffer_obj = buffer_ref_getobject(x->buffer_reference))==NULL){
        error("Buffer %s probably doesn't exist.", name->s_name);
        x->no_buffer = TRUE;
    }else{
        x->no_buffer = FALSE;
        ec2_buffer_limits(x);
    }
}

void ec2_set(t_ec2 *x, t_symbol *s, long ac, t_atom *av){
    defer_low(x, (method)ec2_doset, s, ac, av);
}

void ec2_dblclick(t_ec2 *x){
    //buffer_view(buffer_ref_getobject(x->buffer_reference));
    buffer_view(x->buffer_obj);
}

t_max_err ec2_notify(t_ec2 *x, t_symbol *s, t_symbol *msg, void *sender, void *data){
    //notify the other buffer too, check out what's in s?
    post("%s", s->s_name);
    if(msg==ps_buffer_modified){
        x->buffer_modified = TRUE;
    }
    return buffer_ref_notify(x->buffer_reference, s, msg, sender, data);
}

void ec2_dsp64(t_ec2 *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags){
    x->samplerate = sys_getsr();
    sysmem_copyptr(count, x->count, inlet_amount*sizeof(short));
    
    if(count[inlet_amount-1]){
        object_method(dsp64, gensym("dsp_add64"), x, ec2_perform64_noscan, 0, NULL);
    }else{
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

extern void calculate_windows(t_ec2 *x){
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
