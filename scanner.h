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

#ifndef scanner_h
#define scanner_h

typedef struct _scanner{
	t_sample index;
	t_sample position;
	t_sample begin;
	t_sample range;
	t_sample speed;
	t_sample end;
	t_sample out;
	t_sample begin_out;
	t_sample end_out;
	void (*scan)(struct _scanner*, t_sample, t_atom_long);
}t_scanner;

void scanner_external(t_scanner *scp, t_sample scan, t_atom_long buffer_size){
	scp->out		= scan;
	scp->position 	= scan * buffer_size;

	scp->begin		= 0;
	scp->end		= buffer_size;

	scp->begin_out	= 0;
	scp->end_out	= 1;
}

void scanner_internal(t_scanner *scp, t_sample scan, t_atom_long buffer_size){
	//SCANNER
	//we're off one sample somwhere in scan logic
	//scan logic in general is sometimes not right
	scp->index += scp->speed;
	scp->begin = CLAMP(scp->begin, 0, 1)*buffer_size;
	scp->range = CLAMP(scp->range, 0, 1)*buffer_size;
	//overflow
	scp->index = fmod(scp->index, scp->range+1);
	//underflow
	scp->index = (scp->index<0)?scp->range:scp->index;
	scp->position = fmod(scp->index + scp->begin, buffer_size+1);

	scp->end = fmod(scp->range + scp->begin, buffer_size+1);
	scp->out = (t_sample)fmod(scp->index + scp->begin, buffer_size+1)/buffer_size;
	scp->end_out = (t_sample)scp->end/buffer_size;
}

void scanner_init(t_scanner *scp, t_sample position){
	scp->index = position;
	scp->scan = scanner_internal;
}

#endif
