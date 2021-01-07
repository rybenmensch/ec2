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
    //um die playback-funktion einzugrenzen
    t_sample scan_begin;
    t_sample scan_end;
    
    t_sample out_l;
    t_sample out_r;
}t_voice;

typedef struct _ec2 {
    t_pxobject p_ob;
    t_float samplerate;
    t_buffer_ref *l_buffer_reference;
    t_atom_long buffer_size;
    t_atom_long channel_count;
    t_sample *tukey;
    t_sample *expodec;
    t_sample *rexpodec;
    t_atom_long window_size;
    t_atom_long total_voices;
    t_atom_long active_voices;
    t_voice *voices;
    t_bool init;
    t_sample scan_count;
    t_sample scan_begin;
    t_sample scan_end;
    
    t_atom_long testcounter;
    t_sample *buffer;
    
    t_bool buffer_modified;
    
    short count[9];
} t_ec2;

t_symbol *ps_buffer_modified;
t_class *ec2_class;

void *ec2_new(t_symbol *s,  long argc, t_atom *argv);
void ec2_free(t_ec2 *x);
void ec2_assist(t_ec2 *x, void *b, long m, long a, char *s);

void ec2_perform64(t_ec2 *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);

void ec2_dsp64(t_ec2 *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags){
    x->samplerate = sys_getsr();
    sysmem_copyptr(count, x->count, 9*sizeof(short));
    object_method(dsp64, gensym("dsp_add64"), x, ec2_perform64, 0, NULL);
}

void ec2_set(t_ec2 *x, t_symbol *s){
    if(!x->l_buffer_reference){
        x->l_buffer_reference = buffer_ref_new((t_object *)x, s);
    }else{
        buffer_ref_set(x->l_buffer_reference, s);
    }
}

void ec2_dblclick(t_ec2 *x){
    buffer_view(buffer_ref_getobject(x->l_buffer_reference));
}

t_max_err ec2_notify(t_ec2 *x, t_symbol *s, t_symbol *msg, void *sender, void *data){
    if(buffer_ref_exists(x->l_buffer_reference)){
        if(msg == ps_buffer_modified){
            x->buffer_modified = TRUE;
        }
        return buffer_ref_notify(x->l_buffer_reference, s, msg, sender, data);
    }else{
        return MAX_ERR_NONE;
    }
}

long ec2_multichanneloutputs(t_ec2 *x, long index){
    if(6==index){
        return x->total_voices;
    }else{
        return 1;
    }
}

#endif /* ec2__h */
