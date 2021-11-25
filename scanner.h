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
	int type;
}t_scanner;

void scanner_external(t_scanner *scanner, t_sample scan, t_atom_long buffer_size){
	scanner->out		= scan;
	scanner->position 	= scan * buffer_size;

	scanner->begin		= 0;
	scanner->end		= buffer_size;

	scanner->begin_out	= 0;
	scanner->end_out	= 1;
}

void scanner_internal(t_scanner *scanner, t_sample scan, t_atom_long buffer_size){
	//SCANNER
	//we're off one sample somwhere in scan logic
	//scan logic in general is sometimes not right
	scanner->index += scanner->speed;
	scanner->begin = CLAMP(scanner->begin, 0, 1)*buffer_size;
	scanner->range = CLAMP(scanner->range, 0, 1)*buffer_size;
	//overflow
	scanner->index = fmod(scanner->index, scanner->range+1);
	//underflow
	scanner->index = (scanner->index<0)?scanner->range:scanner->index;
	scanner->position = fmod(scanner->index + scanner->begin, buffer_size+1);

	scanner->end = fmod(scanner->range + scanner->begin, buffer_size+1);
	scanner->out = (t_sample)fmod(scanner->index + scanner->begin, buffer_size+1)/buffer_size;
	scanner->end_out = (t_sample)scanner->end/buffer_size;
}

void scanner_init(t_scanner *scanner, t_sample position){
	scanner->index = position;
	scanner->type = INTERNAL;
}

void scanner_set_type(t_scanner *scanner, t_bool connected){
	if(scanner->type == INTERNAL || !connected){
		scanner->scan = scanner_internal;
	}else if(scanner->type == EXTERNAL){
		scanner->scan = scanner_external;
	}
}

#endif
