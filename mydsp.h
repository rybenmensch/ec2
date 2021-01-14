/*
EC2CLONE
Copyright (C) Manolo Müller, 2020
 
This file is part of EC2CLONE.

Foobar is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Foobar is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with EC2CLONE.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef mydsp_h
#define mydsp_h
#include "ec2~.h"

extern t_sample mfmod(t_sample x, t_sample y){
    t_sample modulated = x;
    t_sample modulor = y;
    t_sample a;
    a=modulated/modulor;
    a-=(int)a;
    return a*modulor;
}

extern void cospan(t_sample in, t_sample pan, t_sample *out_l, t_sample *out_r){
    t_sample p = (pan*0.5)+0.5;
    t_sample pan_scaled = CLAMP(p, 0, 1)/4.;
    t_sample pan_l = cos(pan_scaled * TWOPI);
    t_sample pan_r = cos((pan_scaled + 0.75) * TWOPI);
    *out_l = in * pan_l;
    *out_r = in * pan_r;
}

//channel of buffer for peek??
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


#endif /* mydsp_h */
