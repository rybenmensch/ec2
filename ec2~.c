#include "ec2~.h"
#include "gen.h"
#include "mydsp.h"

//OPTIMIZATION THOUGHTS
//apparently it's faster if there are no functions in for loops, so check that out

//fmod is expensive apparently
//t_sample mfmod(t_sample x, t_sample y){double a;a=x/y;a-=(int)a; return a*y;}
//as a replacement for fmod?
//or comparison if over threshold and then reset?

//calculating expo and tukey instead of LUT?

//assign defaults/some things in general at block rate?

//could this be faster?
//playback_rate   = (count[1]*playback_rate)+(count[1]*1);

//playback and window without functions
//OPTIMIZE LAST...

//SINGLE STREAM: NO MC INPUTS

/*
 TODO BEFORE OPTIMIZATIONS:
 BUGS:
 IMPORTANT:
 - reading/writing into multi-channel-buffers   => DONE, TEST NOW
 - choose better exponentials - should have the same area under curve as tukey-window to preserve unity gain?
 - see how we can clean up the perform loop (put everything into one for loop)
 - give the option for an external scanner (overriding scan_* inputs,
    different perform routine if external scanner is connected)
NICE-TO-HAVE:
 - bufferhandling for external window functions :(
 */

void ext_main(void *r){
    t_class *c;

    c = class_new("ec2~", (method)ec2_new, (method)ec2_free, sizeof(t_ec2), NULL, A_GIMME, 0);
    class_addmethod(c, (method)ec2_dsp64,     "dsp64",    A_CANT, 0);
    class_addmethod(c, (method)ec2_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)ec2_dblclick, "dblclick", A_CANT, 0);
    class_addmethod(c, (method)ec2_notify, "notify", A_CANT, 0);
    class_addmethod(c, (method)ec2_set, "set", A_GIMME, 0);
    class_addmethod(c, (method)ec2_multichanneloutputs, "multichanneloutputs", A_CANT, 0);

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    ec2_class = c;

    ps_buffer_modified = gensym("buffer_modified");
}

void *ec2_new(t_symbol *s, long argc, t_atom *argv){
    t_ec2 *x = (t_ec2 *)object_alloc(ec2_class);
    
    inlet_amount = 9;
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

    x->window_size = 512;

    x->tukey = (t_sample *)sysmem_newptr(x->window_size*sizeof(t_sample));
    x->expodec = (t_sample *)sysmem_newptr(x->window_size*sizeof(t_sample));
    x->rexpodec = (t_sample *)sysmem_newptr(x->window_size*sizeof(t_sample));
    calculate_windows(x);

    x->total_voices = 64;
    x->active_voices = 0;

    x->voices = (t_voice *)sysmem_newptr(x->total_voices * sizeof(t_voice));

    for(int i=0;i<x->total_voices;i++){
            x->voices[i].is_active = 0;
            x->voices[i].play_phase = 0.;
            x->voices[i].window_phase = 0.;
    }

    x->scan_count = 0;
    x->init = TRUE;
    x->samplerate = 44100;

    x->testcounter = 0;

    x->buffersamps = NULL;
    x->buffer_modified = TRUE;
    x->buffer_size = 1;
    x->buffer_reference = NULL;
    x->no_buffer = TRUE;
    
    ec2_set(x, s, argc, argv);
    return (x);
}

t_sample window(t_ec2 *x, t_voice *v){
    //side effects: increases window_phase, changes is_active (when done), active_voices is decreased
    //window determines the "life time" of a single grain!
    t_sample window_phase = v->window_phase;
    window_phase += v->window_increment;
    v->window_phase = window_phase;
    
    if(window_phase >= x->window_size){
        v->is_active = FALSE;
        x->active_voices--;
        return 0;
    }
    
    t_sample tuk        = peek(x->tukey, x->window_size, window_phase);
    t_sample expo       = peek(x->expodec, x->window_size, window_phase);
    t_sample rexpo      = peek(x->rexpodec, x->window_size, window_phase);
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

t_sample playback(t_ec2 *x, t_voice *v){
    t_sample play_phase     = v->play_phase;
    t_sample scan_begin     = v->scan_begin;
    t_sample scan_end       = v->scan_end;
    t_sample playback_rate  = v->playback_rate;
    
    play_phase  += playback_rate;
    play_phase  = fmod(play_phase, scan_end+1);
    play_phase  = (play_phase<0.)?scan_end:play_phase;
    
    v->play_phase = play_phase;
    
    t_sample peek_point = fmod(play_phase+scan_begin, scan_end);
    t_sample sample = peek(x->buffersamps, x->buffer_size, peek_point);
    return sample;
}

void ec2_perform64(t_ec2 *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam){
    t_atom_long total_voices = x->total_voices;
    
    t_sample *p_trig            = ins[0];
    t_sample *p_playback_rate   = ins[1];
    t_sample *p_scan_begin      = ins[2];
    t_sample *p_scan_range      = ins[3];
    t_sample *p_scan_speed      = ins[4];
    t_sample *p_grain_duration  = ins[5];
    t_sample *p_envelope_shape  = ins[6];
    t_sample *p_pan             = ins[7];
    t_sample *p_amplitude       = ins[8];

    t_sample *out_l             = outs[0];
    t_sample *out_r             = outs[1];
    
    t_sample *numbers_out[total_voices];
    for(int i=0;i<total_voices;i++){
        numbers_out[i]  = outs[i+2];
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

    long n=sampleframes;
    t_atom_long buffer_size = x->buffer_size;
    t_atom_long window_size = x->window_size;
    t_float samplerate      = x->samplerate;
     
    while(n--){
        t_sample trig, playback_rate, scan_begin, scan_range, scan_speed, grain_duration, envelope_shape, pan, amplitude;
        t_sample scan_end, starting_point, scan_count, window_increment;
        
        //increment all pointers, get all values, if not connected, assign defaults
        trig            = *p_trig++;
        playback_rate   = *p_playback_rate++;   playback_rate   = (count[1])?playback_rate:1.;
        scan_begin      = *p_scan_begin++;      scan_begin      = (count[2])?scan_begin:0.;
        scan_range      = *p_scan_range++;      scan_range      = (count[3])?scan_range:1.;
        scan_speed      = *p_scan_speed++;      scan_speed      = (count[4])?scan_speed:1.;
        grain_duration  = *p_grain_duration++;  grain_duration  = (count[5])?grain_duration:100;
        envelope_shape  = *p_envelope_shape++;  envelope_shape  = (count[6])?envelope_shape:0.5;
        pan             = *p_pan++;             pan             = (count[7])?pan:0.;
        amplitude       = *p_amplitude++;       amplitude       = (count[8])?amplitude:1.;
        
        //SCANNER
        //we're off one sample somwhere in scan logic
        //scan logic in general is sometimes not right
        scan_count = x->scan_count;
        if(x->init){
            scan_count = scan_begin;
            x->init = FALSE;
        }
        scan_count += scan_speed;
        
        scan_begin = CLAMP(scan_begin, 0, 1)*buffer_size;
        scan_range = CLAMP(scan_range, 0, 1)*buffer_size;

        //overflow
        scan_count = mfmod(scan_count, scan_range+1);
        //scan_count = (scan_count>=scan_range)?0:scan_count;
        //underflow
        scan_count = (scan_count<0)?scan_range:scan_count;
        scan_end = fmod(scan_range + scan_begin, buffer_size+1);
        
        t_atom_long new_index = 0;
        if(trig>0.){
            if(x->active_voices < x->total_voices){
                x->active_voices++;
                for(int i=0;i<x->total_voices;i++){
                    if(x->voices[i].is_active == FALSE){
                        new_index = i;
                        x->voices[i].is_active = TRUE;
                        break;
                    }
                }
                
                //got our voice, fill in the data
                starting_point = fmod(scan_count + scan_begin, buffer_size+1);
                grain_duration *= (samplerate/1000.);
                
                window_increment = ((t_sample) (window_size))/grain_duration;
                
                x->voices[new_index].scan_begin        = scan_begin;
                x->voices[new_index].scan_end          = scan_end;
                x->voices[new_index].playback_rate     = playback_rate;
                x->voices[new_index].envelope_shape    = CLAMP(envelope_shape, 0, 1);
                x->voices[new_index].pan               = CLAMP(pan, -1, 1);
                x->voices[new_index].amplitude         = CLAMP(amplitude, 0, 1);
                x->voices[new_index].window_increment  = window_increment;
                x->voices[new_index].window_phase      = 0;
                x->voices[new_index].play_phase        = starting_point;
            }
        }
        
        t_sample accum_l = 0;
        t_sample accum_r = 0;
                
        for(int i=0;i<x->total_voices;i++){
            if(x->voices[i].is_active == TRUE){
                t_voice *v = &(x->voices[i]);
                t_sample windowsamp     = window(x, v);
                t_sample playbacksamp   = playback(x, v);
                
                playbacksamp *= windowsamp;
                playbacksamp *= x->voices[i].amplitude;
                //normalization by total amount of voices
                playbacksamp *= (t_sample)1./x->total_voices;
                t_sample pan_l, pan_r;
                cospan(playbacksamp, v->pan, &pan_l, &pan_r);
                accum_l += pan_l;
                accum_r += pan_r;
            }
        }
        
        *out_l++ = FIX_DENORM_NAN_SAMPLE(accum_l);
        *out_r++ = FIX_DENORM_NAN_SAMPLE(accum_r);
        for(int i=0;i<x->total_voices;i++){
            t_bool sali = x->voices[i].is_active;
            *numbers_out[i]++ = sali;
        }

        *scan_out++ = (t_sample)fmod(scan_count + scan_begin, buffer_size+1)/buffer_size;
        *scan_begin_out++ = (t_sample) scan_begin/buffer_size;
        *scan_end_out++ = (t_sample )scan_end/buffer_size;

        //reassign cached values that update sample wise HERE:
        x->scan_count = scan_count;
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
        }
    }else{
        switch(a){
            case 0:
                sprintf(s, "(signal) Left output per stream");
                break;
            case 1:
                sprintf(s, "(signal) Right output per stream");
                break;
            case 2:
                sprintf(s, "(mcsignal) Busymap");
                break;
            case 3:
                sprintf(s, "(mcsignal) Scan");
                break;
        }
    }
}
