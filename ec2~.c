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

#include "ec2~.h"

//OPTIMIZATION THOUGHTS
//apparently it's faster if there are no functions in for loops, so check that out

//fmod is expensive apparently
//t_sample mfmod(t_sample x, t_sample y){double a;a=x/y;a-=(int)a; return a*y;}
//as a replacement for fmod?
//or comparison if over threshold and then reset?
//APPARENTLY, fmod is cheaper than comparison over threshhold

//calculating expo and tukey instead of LUT?

//assign defaults/some things in general at block rate?

//could this be faster?
//playback_rate   = (count[1]*playback_rate)+(count[1]*1);

//playback and window without functions
//OPTIMIZE LAST...

/*
TODO BEFORE OPTIMIZATIONS:
BUGS:
- crashes sometimes when re-setting the external window buffer
	=> this also affects the regular playback
IMPORTANT:
- extwindow buffer notifying on change
- see how we can clean up the perform loop (put everything into one for loop)
NICE-TO-HAVE:
- second extwindow to be able to interpolate between two buffers (double function of envtype)
- choose better exponentials - should have the same area under curve as tukey-window to preserve unity gain?
- windowed sync interpolation for playback (probably less clicky)
- copyright? genlib doesn't have an open source licence
*/

/*
reihenfolge grain-initialisation - synthese sollte umgekehrt sein
zuerst synthese, dann assignment
grund: grains die in einem vektor zu ende gehen können sofort reassigned werden
*/

/*
vielleicht wäre es besser, wenn innerhalb der perform-routinen alles im bereich 0 1 oder -1 1 abläuft
zb scan, dann kann das verlängern auf die buffersize in der implementation gemacht werden
*/

void ext_main(void *r){
	t_class *c;

	c = class_new("ec2~", (method)ec2_new, (method)ec2_free, sizeof(t_ec2), NULL, A_GIMME, 0);
	class_addmethod(c, (method)ec2_dsp64,     	"dsp64",    A_CANT, 0);
	class_addmethod(c, (method)ec2_assist,		"assist", A_CANT, 0);
	class_addmethod(c, (method)ec2_dblclick,	"dblclick", A_CANT, 0);
	class_addmethod(c, (method)ec2_notify,		"notify", A_CANT, 0);
	class_addmethod(c, (method)ec2_mcout,		"multichanneloutputs", A_CANT, 0);
	class_addmethod(c, (method)ec2_set,			"set", A_GIMME, 0);
	class_addmethod(c, (method)ec2_window_ext,	"windowext", A_GIMME, 0);
	class_addmethod(c, (method)ec2_window_type, "windowtype", A_SYM, 0);
	class_addmethod(c, (method)ec2_scan_type,	"scantype", A_SYM, 0);
	class_addmethod(c, (method)ec2_norm,		"normalization", A_LONG, 0);
	class_addmethod(c, (method)ec2_glisson,		"glisson", A_GIMME, 0);
	class_addmethod(c, (method)ec2_glissonr,	"glissonr", A_GIMME, 0);

	class_dspinit(c);
	class_register(CLASS_BOX, c);
	ec2_class = c;

	ps_buffer_modified = gensym("buffer_modified");
}

void *ec2_new(t_symbol *s, long argc, t_atom *argv){
    t_ec2 *x = (t_ec2 *)object_alloc(ec2_class);

    inlet_amount = 10;
    x->count = (short*)sysmem_newptr(inlet_amount*sizeof(short));

    dsp_setup((t_pxobject * )x, inlet_amount);
    //scan outlets, voice active map (mc)
    for(int i=0;i<2;i++){
         outlet_new((t_object *)x, "multichannelsignal");
     }
    //l out, r out
    for(int i=0;i<2;i++){
        outlet_new((t_object *)x, "signal");
    }

	scanner_init(&x->scanner, 0.);
	window_init(&x->window, 512);

	//voices_init(x, 64);
    x->total_voices = 64;
    x->active_voices = 0;
    x->voices = (t_voice *)sysmem_newptr(x->total_voices * sizeof(t_voice));
    for(int i=0;i<x->total_voices;i++){
		x->voices[i].is_active		= FALSE;
		x->voices[i].is_done		= FALSE;
		x->voices[i].play_phase		= 0.;
		x->voices[i].window_phase	= 0.;
    }

    x->samplerate = sys_getsr();


	x->glisson[0] = x->glisson_inv[0] = 0;
	x->glisson[1] = x->glisson_inv[1] = 0;
	x->glisson_rand = FALSE;

    x->buffersamps = NULL;
    x->buffer_modified = TRUE;
    x->buffer_size = 1;
    x->buffer_reference = NULL;
    x->no_buffer = TRUE;
    ec2_set(x, s, argc, argv);

    ec2_norm(x, 1);

	srand((uint)time(NULL));

    return (x);
}

t_sample glisson(t_ec2 *x, t_voice *v){
	t_sample window_phase = v->window_phase / (x->window_size-1);
	t_sample glisson_offset = (window_phase * x->glisson[0]) + x->glisson[1];
	return glisson_offset;
}

t_sample playback(t_ec2 *x, t_voice *v){
    t_sample play_phase     = v->play_phase;
    t_sample scan_begin     = v->scan_begin;
    t_sample scan_end       = v->scan_end;
    t_sample playback_rate  = v->playback_rate;

	//glissons are easy as fuck
	playback_rate += glisson(x, v);
    play_phase  += playback_rate;
    play_phase  = fmod(play_phase, scan_end+1);
    play_phase  = (play_phase<0.)?scan_end:play_phase;

    v->play_phase = play_phase;

    t_sample peek_point = fmod(play_phase+scan_begin, scan_end);
    t_sample sample = peek(x->buffersamps, x->buffer_size, peek_point);
    return sample;
}

t_sample window_direct(t_ec2 *x, t_voice *v){
    //trying this out, because apparently memory access is the slowest thing a computer can do nowadays
    //side effects: increases window_phase, changes is_active (when done), active_voices is decreased
    //window determines the "life time" of a single grain!
    t_sample window_phase = v->window_phase;
    window_phase += v->window_increment;
    v->window_phase = window_phase;
    t_atom_long size = x->window_size;

    if(window_phase >= size){
        v->is_active = FALSE;
        x->active_voices--;
        return 0;
    }

    t_sample env_shape  = v->envelope_shape;

    t_sample alpha = 0.5;
    t_sample anm12 = 0.5*alpha*size;
    t_sample tuk = 0;

    if(window_phase<=anm12){
        tuk = 0.5*(1+cos(PI*(window_phase/anm12 - 1)));
    }else if(window_phase<size*(1-0.5*alpha)){
        tuk = 1;
    }else{
        tuk = 0.5*(1+cosf(PI*(window_phase/anm12 - 2/alpha + 1)));
    }

    t_sample interp = 0;
    if(env_shape<0.5){
        t_sample phase = (size-window_phase)/(t_sample)(size-1);
        t_sample expo = (pow(36, phase)-1)/(35);
        interp = ((expo * (1-env_shape*2)) + (tuk * env_shape * 2));
    }else if(env_shape==0.5){
        interp = tuk;
    }else if(env_shape<=1.){
        t_sample phase = window_phase/(t_sample)(size-1);
        t_sample rexpo = (pow(36, phase)-1)/(35);
        interp = ((tuk * (1 - (env_shape - 0.5) * 2)) + (rexpo * (env_shape - 0.5) * 2));
    }else{
        interp = tuk;
    }
    return interp;
}

t_sample window_external(t_window *w, t_voice *v){
    //side effects: increases window_phase, changes is_active (when done), active_voices is decreased
    //window determines the "life time" of a single grain!
    t_sample window_phase = v->window_phase;
    window_phase += v->window_increment;
    v->window_phase = window_phase;

    if(window_phase >= w->size){
        v->is_active = FALSE;
		v->is_done = TRUE;
		//x->active_voices--;
        return 0;
    }

    //t_sample samp = peek(w->window_ext_samps, w->size, window_phase);
    //return samp;
	return 0;
}

t_sample window_internal(t_window *w, t_voice *v){
    //side effects: increases window_phase, changes is_active (when done), active_voices is decreased
    //window determines the "life time" of a single grain!
    t_sample window_phase = v->window_phase;
    window_phase += v->window_increment;
    v->window_phase = window_phase;

    if(window_phase >= w->size){
        v->is_active = FALSE;
		v->is_done = TRUE;
        //x->active_voices--;
        return 0;
    }

    t_sample tuk        = peek(w->tukey,	w->size, window_phase);
    t_sample expo       = peek(w->expodec,	w->size, window_phase);
    t_sample rexpo      = peek(w->rexpodec, w->size, window_phase);
    t_sample env_shape  = v->envelope_shape;

    t_sample interp = 0;
    if(env_shape<0.5){
        interp = ((expo * (1-env_shape*2)) + (tuk * env_shape * 2));
    }else if(env_shape==0.5){
        interp = tuk;
    }else if(env_shape<=1.){
        interp = ((tuk * (1 - (env_shape - 0.5) * 2)) + (rexpo * (env_shape - 0.5) * 2));
    }else{
        interp = tuk;
    }
    return interp;
}

void ec2_perform64(t_ec2 *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam){
    t_sample *p_trig            = ins[0];
    t_sample *p_playback_rate   = ins[1];
    t_sample *p_scan_begin      = ins[2];
    t_sample *p_scan_range      = ins[3];
    t_sample *p_scan_speed      = ins[4];
    t_sample *p_grain_duration  = ins[5];
    t_sample *p_envelope_shape  = ins[6];
    t_sample *p_pan             = ins[7];
    t_sample *p_amplitude       = ins[8];
    t_sample *p_scan            = ins[9];

    t_sample *out_l             = outs[0];
    t_sample *out_r             = outs[1];

    t_atom_long total_voices = x->total_voices;
    t_sample *numbers_out[total_voices];
    for(int i=0;i<total_voices;i++){
        numbers_out[i] 			= outs[i+2];
    }

    t_sample *scan_out          = outs[total_voices+2];
    t_sample *scan_begin_out    = outs[total_voices+3];
    t_sample *scan_end_out      = outs[total_voices+4];

    short count[inlet_amount];
    sysmem_copyptr(x->count, count, inlet_amount*sizeof(short));

    if(x->buffer_modified){
        ec2_buffer_limits(x);
        x->buffer_modified = FALSE;
    }

     if(x->no_buffer){
         goto zero;
     }

	t_window *w = &x->window;
	t_scanner *scp = &x->scanner;

    t_atom_long n			= sampleframes;
    t_atom_long buffer_size = x->buffer_size;
    t_atom_long window_size = w->size;
    t_float samplerate      = x->samplerate;

    while(n--){
        t_sample trig, playback_rate, scan_begin, scan_range, scan_speed, grain_duration, envelope_shape, pan, amplitude, scan;
		t_sample window_increment;

        //increment all pointers, get all values, if not connected, assign defaults
        trig            = *p_trig++;
        playback_rate   = *p_playback_rate++;   playback_rate   = (count[1])?playback_rate:1.;
        scan_begin      = *p_scan_begin++;      scp->begin 		= (count[2])?scan_begin:0.;
        scan_range      = *p_scan_range++;      scp->range 		= (count[3])?scan_range:1.;
        scan_speed      = *p_scan_speed++;      scp->speed 		= (count[4])?scan_speed:1.;
        grain_duration  = *p_grain_duration++;  grain_duration  = (count[5])?grain_duration:100;
        envelope_shape  = *p_envelope_shape++;  envelope_shape  = (count[6])?envelope_shape:0.5;
        pan             = *p_pan++;             pan             = (count[7])?pan:0.;
        amplitude       = *p_amplitude++;       amplitude       = (count[8])?amplitude:1.;
		scan			= *p_scan++;			scan			= (count[9])?scan:0.;
		scp->scan(scp, scan, buffer_size);

        if(trig>0. && amplitude!=0){
			t_atom_long new_index = 0;
            if(x->active_voices < x->total_voices){
                x->active_voices++;
                for(int i=0;i<x->total_voices;i++){
                    if(x->voices[i].is_active == FALSE){
                        new_index = i;
                        x->voices[i].is_active	= TRUE;
						x->voices[i].is_done	= FALSE;
                        break;
                    }
                }

                //got our voice, fill in the data
                grain_duration *= (samplerate/1000.);
                window_increment = ((t_sample) (window_size))/grain_duration;

                x->voices[new_index].scan_begin        = scp->begin;
                x->voices[new_index].scan_end          = scp->end;
                x->voices[new_index].playback_rate     = playback_rate;
                x->voices[new_index].envelope_shape    = CLAMP(envelope_shape, 0, 1);
                x->voices[new_index].pan               = CLAMP(pan, -1, 1);
                x->voices[new_index].amplitude         = CLAMP(amplitude, 0, 1);
                x->voices[new_index].window_increment  = window_increment;
                x->voices[new_index].window_phase      = 0;
                x->voices[new_index].play_phase        = scp->position;
            }
        }

		//SYNTHESIZE
        t_sample accum_l = 0;
        t_sample accum_r = 0;
        for(int i=0;i<x->total_voices;i++){
            if(x->voices[i].is_active == TRUE){
                t_voice *v = &(x->voices[i]);
				t_sample windowsamp = w->window(w, v);
				t_sample playbacksamp   = playback(x, v);
                playbacksamp *= windowsamp;
                playbacksamp *= x->voices[i].amplitude;
				//normalization by total amount of voices
                playbacksamp *= x->norm;

                t_sample pan_l, pan_r;
                cospan(playbacksamp, v->pan, &pan_l, &pan_r);
				accum_l += pan_l;
                accum_r += pan_r;

				x->active_voices -= v->is_done;
            }
			//busymap
			*numbers_out[i]++ = x->voices[i].is_active;
        }

        *out_l++ = FIX_DENORM_NAN_SAMPLE(accum_l);
        *out_r++ = FIX_DENORM_NAN_SAMPLE(accum_r);

		*scan_out++ 		= scp->out;
		*scan_begin_out++	= scp->begin_out;
		*scan_end_out++		= scp->end_out;
        //reassign cached values that update sample wise HERE:
    }

    //reassign cached values that update in block size HERE:
    return;
zero:
    for(int i=0;i<numouts;i++){
        set_zero64(outs[i], sampleframes);
    }
}

void ec2_free(t_ec2 *x){
    dsp_free((t_pxobject *)x);
    object_free(x->buffer_reference);
    object_free(x->window_ext_ref);

    if(x->tukey){
        sysmem_freeptr(x->tukey);
    }
    if(x->expodec){
        sysmem_freeptr(x->expodec);
    }
    if(x->rexpodec){
        sysmem_freeptr(x->rexpodec);
    }

    if(x->voices){
        sysmem_freeptr(x->voices);
    }

    if(x->buffersamps){
        sysmem_freeptr(x->buffersamps);
    }

    if(x->window_ext_samps){
        sysmem_freeptr(x->window_ext_samps);
    }

    if(x->count){
        sysmem_freeptr(x->count);
    }
}

void ec2_assist(t_ec2 *x, void *b, long m, long a, char *s){
    if(m == ASSIST_INLET){
        switch(a){
            case 0:
                sprintf(s, "(signal) Trigger");
                break;
            case 1:
                sprintf(s, "(signal) Playback rate");
                break;
            case 2:
                sprintf(s, "(signal) Scan begin (0. - 1.)");
                break;
            case 3:
                sprintf(s, "(signal) Scan range (0. - 1.)");
                break;
            case 4:
                sprintf(s, "(signal) Scan speed");
                break;
            case 5:
                sprintf(s, "(signal) Grain duration (ms)");
                break;
            case 6:
                sprintf(s, "(signal) Envelope shape (0. - 1.)");
                break;
            case 7:
                sprintf(s, "(signal) Pan (-1. - 1.)");
                break;
            case 8:
                sprintf(s, "(signal) Amplitude (0. - 1.)");
                break;
            case 9:
                sprintf(s, "(signal) External scan position (0. - 1.)");
        }
    }else{
        switch(a){
            case 0:
                sprintf(s, "(signal) Left output");
                break;
            case 1:
                sprintf(s, "(signal) Right output");
                break;
            case 2:
                sprintf(s, "(mcsignal) Busymap");
                break;
            case 3:
                sprintf(s, "(mcsignal) Scanhead and range");
                break;
        }
    }
}
