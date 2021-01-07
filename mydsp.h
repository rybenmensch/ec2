#ifndef mydsp_h
#define mydsp_h
#include "ec2~.h"

extern void cospan(t_sample in, t_sample pan, t_sample *out_l, t_sample *out_r){
    pan = (pan*0.5)+0.5;
    t_sample pan_scaled = CLAMP(pan, 0, 1)/4.;
    t_sample pan_l = cos(pan_scaled * TWOPI);
    t_sample pan_r = cos((pan_scaled + 0.75) * TWOPI);
    *out_l = in * pan_l;
    *out_r = in * pan_r;
}

extern void cospano(t_sample in, t_sample pan, t_sample *out_l, t_sample *out_r){
    pan = (pan*0.5)+0.5;
    t_sample pan_scaled = CLAMP(pan, 0, 1)/4.;
    t_sample pan_l = cos(pan_scaled * TWOPI);
    t_sample pan_r = cos((pan_scaled + 0.75) * TWOPI);
    *out_l = in * pan_l;
    *out_r = in * pan_r;
}

extern t_sample peek(t_sample *buf, t_atom_long buffer_size, t_sample index){
    //index in samples (mit fract)
    t_atom_long index_trunc = floor(index);
    
    t_sample index_fract = index - index_trunc;
    index_trunc++;
    t_bool index_ignore = ((index_trunc >= buffer_size) || (index_trunc<0));
    
    t_sample read = (index_ignore)?0:(t_sample)buf[index_trunc];
    t_sample readinterp = cosine_interp(index_fract, read, read);
    return readinterp;
}

extern float fpeek(t_float *buf, t_atom_long buffer_size, t_float index){
    //index in samples (mit fract)
    t_atom_long index_trunc = floor(index);
    
    t_float index_fract = index - index_trunc;
    index_trunc++;
    t_bool index_ignore = ((index_trunc >= buffer_size) || (index_trunc<0));
    
    t_float read = (index_ignore)?0:buf[index_trunc];
    t_float readinterp = cosine_interp(index_fract, read, read);
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

#endif /* mydsp_h */
