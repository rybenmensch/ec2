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

#ifndef mydsp_h
#define mydsp_h
#include "common.h"
extern t_sample mfmod(t_sample x, t_sample y){
    t_sample modulated = x;
    t_sample modulor = y;
    t_sample a;
    a=modulated/modulor;
    a-=(int)a;
    return a*modulor;
}

extern t_sample transratio(t_sample x){
	return exp(.057762265 * x);
}

extern void cospan(t_sample in, t_sample pan, t_sample *out_l, t_sample *out_r){
    t_sample p = (pan*0.5)+0.5;
    t_sample pan_scaled = CLAMP(p, 0, 1)/4.;
    t_sample pan_l = cos(pan_scaled * TWOPI);
    t_sample pan_r = cos((pan_scaled + 0.75) * TWOPI);
    *out_l = in * pan_l;
    *out_r = in * pan_r;
}

/*
 TODO: replace with GPL code or own invention
 */

extern t_sample lpeek(t_sample *buf, t_atom_long buffer_size, t_sample index){
    t_atom_long index_trunc = floor(index);
    t_sample index_fract = (index - index_trunc);
    t_atom_long index_trunc_2 = (index_trunc+1);
    t_bool index_ignore = ((index_trunc >= buffer_size) || (index_trunc<0));
    t_bool index_ignore_2 = ((index_trunc_2 >= buffer_size) || (index_trunc_2<0));

    t_sample read = (index_ignore)?0:buf[index_trunc];
    t_sample read_2 = (index_ignore_2)?0:buf[index_trunc_2];

    t_sample readinterp = linear_interp(index_fract, read, read_2);
    return readinterp;
}

extern t_sample peek(t_sample *buf, t_atom_long buffer_size, t_sample index){
    t_atom_long index_trunc = floor(index);
    t_sample index_fract = (index - index_trunc);
    t_atom_long index_trunc_2 = (index_trunc + 1);
    t_bool index_ignore = ((index_trunc >= buffer_size) || (index_trunc < 0));
    t_bool index_ignore_2 = ((index_trunc_2 >= buffer_size) || (index_trunc_2 < 0));

    t_sample read = index_ignore ?0:buf[index_trunc];
    t_sample read_2 = index_ignore_2?0:buf[index_trunc_2];
    t_sample readinterp = cosine_interp(index_fract, read, read_2);
    return readinterp;
    /*
     //this is "wrong" from the side of the code, but the sound was good, so i'm keeping it
     //commented out :)
    t_atom_long index_trunc = floor(index);
    
    t_sample index_fract = index - index_trunc;
    index_trunc++;
    t_bool index_ignore = ((index_trunc >= buffer_size) || (index_trunc<0));
    
    t_sample read = (index_ignore)?0:(t_sample)buf[index_trunc];
    t_sample readinterp = cosine_interp(index_fract, read, read);
    return readinterp;
     */
}

extern t_sample stereo_peek(t_sample *buf, t_atom_long buffer_size, t_sample index){
    t_atom_long index_trunc = floor(index);
    t_sample index_fract = (index - index_trunc);
    t_atom_long index_trunc_2 = (index_trunc + 1);
    t_bool index_ignore = ((index_trunc >= buffer_size) || (index_trunc < 0));
    t_bool index_ignore_2 = ((index_trunc_2 >= buffer_size) || (index_trunc_2 < 0));

    t_sample read_l = index_ignore ?0:buf[index_trunc];
    t_sample read_2_l = index_ignore_2?0:buf[index_trunc_2];
    t_sample readinterp_l = cosine_interp(index_fract, read_l, read_2_l);
    return readinterp_l;
}

enum svf_type {LOWPASS = 0, HIGHPASS, BANDPASS, NOTCH, BYPASS};
typedef struct _svf{
	t_sample freq;
	t_sample q;
	t_sample delay1;
	t_sample delay2;
	t_sample inv_sr;
	enum svf_type type;
	t_bool is_enabled;
}t_svf;

typedef struct _svf_gliss{
	t_sample m;
	t_sample b;
	t_sample center;
	t_sample q;
}t_svf_gliss;

//CHANGE INVSR TO FILTERCOEFF 2_PI/sr

void svf_set_frequency(t_svf *svf, t_sample freq){
	freq		= CLAMP(freq, 1, 20000);
	svf->freq	= freq;
}

void svf_set_q(t_svf *svf, t_sample q){
	q			= CLAMP(q, 0.5, 100);
	svf->q		= q;
}

void svf_set_type(t_svf *svf, enum svf_type type){
	svf->type = type;
}

void svf_init(t_svf *svf, t_sample freq, t_sample q, enum svf_type type, t_sample inv_sr){
	svf->inv_sr = inv_sr;
	svf_set_frequency(svf, freq);
	svf_set_q(svf, q);
	svf_set_type(svf, type);
	svf->delay1 = 0;
	svf->delay2 = 0;
	svf->is_enabled = TRUE;
}

void svf_reset(t_svf *svf, t_sample inv_sr){
	svf->delay1 = 0;
	svf->delay2 = 0;
	svf->inv_sr = inv_sr;
}

t_sample svf_process(t_svf *svf, t_sample in){
	if(!svf->is_enabled){
		return in;
	}

	t_sample q1 = 1. / svf->q;
	t_sample f1 = sin(2*M_PI*svf->freq * svf->inv_sr);

	t_sample L = svf->delay2 + f1*svf->delay1;
	t_sample H = in - L - q1*svf->delay1;
	t_sample B = f1 * H + svf->delay1;
	t_sample N = H+L;

	// store delay:
	svf->delay1 = B;
	svf->delay2 = L;

	t_sample result = 0;

	switch(svf->type){
		case LOWPASS:
			result = L;
			break;
		case HIGHPASS:
			result = H;
			break;
		case BANDPASS:
			result = B;
			break;
		case NOTCH:
			result = N;
			break;
		case BYPASS:
			result = in;
			break;
		default:
			result = 0;
	}
	return result;
}

#endif /* mydsp_h */
