#ifndef ec2__h
#define ec2__h
#include "ext.h"
#include "ext_obex.h"
#include "ext_common.h"
#include "ext_buffer.h"
#include "z_dsp.h"

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
    t_bool is_active;
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
    
    t_sample *tukey;
    t_sample *expodec;
    t_sample *rexpodec;
    t_atom_long window_size;
    
    t_atom_long total_voices;
    t_atom_long active_voices;
    t_atom_long total_streams;
    t_atom_long active_streams;
    t_voice *voices;
    t_stream *streams;
    
    t_bool init;
    t_sample scan_count;
    t_atom_long testcounter;
    
    short count[9];
    t_atom_long input_count;
} t_ec2;

t_symbol *ps_buffer_modified;
t_class *ec2_class;

void *ec2_new(t_symbol *s,  long argc, t_atom *argv);
void ec2_free(t_ec2 *x);
void ec2_assist(t_ec2 *x, void *b, long m, long a, char *s);

void ec2_perform64(t_ec2 *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);

long ec2_inputchanged(t_ec2 *x, long index, long count){
    if(count != x->input_count){
        post("number of channels has changed from %ld to %ld", x->input_count, count);
        x->input_count = count;
        return TRUE;
    }else{
        return FALSE;
    }
}

long ec2_multichanneloutputs(t_ec2 *x, long index){
    if(6==index){
        return x->total_voices;
    }else{
        return 1;
    }
}

void ec2_dsp64(t_ec2 *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags){
    x->samplerate = sys_getsr();
    sysmem_copyptr(count, x->count, 9*sizeof(short));
    x->input_count = (t_atom_long)object_method(dsp64, gensym("getnuminputchannels"), x, 0);
    object_method(dsp64, gensym("dsp_add64"), x, ec2_perform64, 0, NULL);
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
            x->buffersamps[i] = (t_sample)buffersamps[i];
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
        post("Buffer %s probably doesn't exist.", name->s_name);
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
    buffer_view(buffer_ref_getobject(x->buffer_reference));
}

t_max_err ec2_notify(t_ec2 *x, t_symbol *s, t_symbol *msg, void *sender, void *data){
    if(msg==ps_buffer_modified){
        x->buffer_modified = TRUE;
    }
    return buffer_ref_notify(x->buffer_reference, s, msg, sender, data);
}



#endif /* ec2__h */
