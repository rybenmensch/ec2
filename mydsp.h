#ifndef mydsp_h
#define mydsp_h
#include "ec2~.h"

extern void pan(t_sample in, t_sample pan, t_sample *out_l, t_sample *out_r){
    pan = (pan*0.5)+0.5;
    t_sample pan_scaled = CLAMP(pan, 0, 1)/4.;
    t_sample pan_l = cos(pan_scaled * TWOPI);
    t_sample pan_r = cos((pan_scaled + 0.75) * TWOPI);
    *out_l = in * pan_l;
    *out_r = in * pan_r;
}

extern t_sample peek(t_atom_long window_size, t_sample *buf, t_sample index){
    //index in samples (mit fract)
    int index_trunc = floor(index);
    
    t_sample index_fract = index - index_trunc;
    index_trunc++;
    t_bool index_ignore = ((index_trunc >= window_size) || (index_trunc<0));
    
    t_sample read = (index_ignore)?0:buf[index_trunc];
    t_sample readinterp = cosine_interp(index_fract, read, read);
    return readinterp;
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

extern t_sample windowsamp(t_ec2 *x, t_atom_long voice_index, t_sample index){
    t_sample tuk    = peek(x->window_size, x->tukey, index);
    t_sample expo   = peek(x->window_size, x->expodec, index);
    t_sample rexpo  = peek(x->window_size, x->rexpodec, index);
    t_sample val = 0;
    t_sample env_shape = x->voices[voice_index].envelope_shape;
    
    if(env_shape <0.5){
        val = ((expo * (1-env_shape*2)) + (tuk * env_shape * 2));
    }else if(env_shape==0.5){
        val = tuk;
    }else if(env_shape<=1.){
        val = ((tuk * (1 - (env_shape - 0.5) * 2)) + (rexpo * (env_shape - 0.5) * 2));
    }else{
        val = tuk;
    }
    return val;
}


#endif /* mydsp_h */
