#include "ec2~.h"
#include "gen.h"
#include "mydsp.h"

//OPTIMIZATION THOUGHTS
//apparently it's faster if there are no functions in for loops, so check that out

//t_sample mfmod(t_sample x, t_sample y){double a;a=x/y;a-=(int)a; return a*y;}
//as a replacement for fmod?
//or comparison if over threshold and then reset?

//calculating expo and tukey instead of LUT?

//fast cos in panning function? maybe not as gen~ itself doesn't do that in the exported cospan.gendsp

//assign defaults/some things in general at block rate?

//could this be faster?
//playback_rate   = (count[1]*playback_rate)+(count[1]*1);
//OPTIMIZE LAST...

void ext_main(void *r){
    t_class *c;

    c = class_new("ec2~", (method)ec2_new, (method)ec2_free, sizeof(t_ec2), NULL, A_GIMME, 0);
    class_addmethod(c, (method)ec2_dsp64,     "dsp64",    A_CANT, 0);
    class_addmethod(c, (method)ec2_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)ec2_dblclick, "dblclick", A_CANT, 0);
    class_addmethod(c, (method)ec2_notify, "notify", A_CANT, 0);
    //class_addmethod(c, (method)ec2_set, "set", A_SYM, 0);
    class_addmethod(c, (method)ec2_set, "set", A_GIMME, 0);
    class_addmethod(c, (method)ec2_multichanneloutputs, "multichanneloutputs", A_CANT, 0);
    class_addmethod(c, (method)ec2_inputchanged, "inputchanged", A_CANT, 0);


    class_dspinit(c);
    class_register(CLASS_BOX, c);
    ec2_class = c;

    ps_buffer_modified = gensym("buffer_modified");
}

void *ec2_new(t_symbol *s, long argc, t_atom *argv){
    t_ec2 *x = (t_ec2 *)object_alloc(ec2_class);
    dsp_setup((t_pxobject * )x, 9);
    outlet_new((t_object *)x, "multichannelsignal");

    for(int i=0;i<6;i++){
        outlet_new((t_object *)x, "signal");
    }

    x->window_size = 512;

    x->tukey = (t_sample *)sysmem_newptr(x->window_size*sizeof(t_sample));
    x->expodec = (t_sample *)sysmem_newptr(x->window_size*sizeof(t_sample));
    x->rexpodec = (t_sample *)sysmem_newptr(x->window_size*sizeof(t_sample));
    calculate_windows(x);

    x->total_voices = 64;
    x->active_voices = 0;

    x->total_streams = 12;
    x->active_streams = 1;
    
    /*
    x->voices = (t_voice *)sysmem_newptr(x->total_voices * sizeof(t_voice));
    for(int i=0;i<x->total_voices;i++){
        x->voices[i].is_active = 0;
        x->voices[i].play_phase = 0.;
        x->voices[i].window_phase = 0.;
    }
    */
     
    x->streams = (t_stream *)sysmem_newptr(x->total_streams * sizeof(t_voice));
    for(int i=0;i<x->total_streams;i++){
        x->streams[i].is_active = (i==0);   //activate first stream only
        x->streams[i].active_voices = 0;
        x->streams[i].voices = (t_voice *)sysmem_newptr(x->total_voices * sizeof(t_voice));

        for(int j=0;j<x->total_voices;j++){
            x->streams[i].voices[j].is_active = 0;
            x->streams[i].voices[j].play_phase = 0.;
            x->streams[i].voices[j].window_phase = 0.;
        }
    }

    x->scan_count = 0;
    x->init = TRUE;
    x->samplerate = 44100;

    x->testcounter = 0;
    x->input_count = 1;

    x->buffersamps = NULL;
    x->buffer_modified = TRUE;
    x->buffer_size = 1;
    x->buffer_reference = NULL;
    x->no_buffer = TRUE;
    
    ec2_set(x, s, argc, argv);
    return (x);
}

t_sample window(t_ec2 *x, t_atom_long voice_index){
    //side effects: increases window_phase, changes is_active (when done), active_voices is decreased
    //window determines the "life time" of a single grain!
    t_atom_long v_i = voice_index;
    t_sample window_phase = x->voices[v_i].window_phase;
    window_phase += x->voices[v_i].window_increment;
    x->voices[v_i].window_phase = window_phase;

    if(window_phase>=x->window_size){
        x->voices[v_i].is_active = FALSE;
        x->active_voices--;
        return 0;
    }

    t_sample tuk        = peek(x->tukey, x->window_size, window_phase);
    t_sample expo       = peek(x->expodec, x->window_size, window_phase);
    t_sample rexpo      = peek(x->rexpodec, x->window_size, window_phase);
    t_sample env_shape  = x->voices[v_i].envelope_shape;

    t_sample interp = 0;
    if(env_shape <0.5){
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

t_sample playback(t_ec2 *x, t_atom_long voice_index){
    t_sample play_phase     = x->voices[voice_index].play_phase;
    t_sample scan_begin     = x->voices[voice_index].scan_begin;
    t_sample scan_end       = x->voices[voice_index].scan_end;
    t_sample playback_rate  = x->voices[voice_index].playback_rate;

    play_phase += playback_rate;
    //loop between 0 and scan_end, then add the offset for scan_begin when peeking
    //overflow
    play_phase = fmod(play_phase, scan_end+1);  //are we off-by-one? *shrug*
    //underflow
    play_phase = (play_phase<0.)?scan_end:play_phase;
    x->voices[voice_index].play_phase = play_phase;
    t_sample peek_point = fmod(play_phase+scan_begin, scan_end);    //vorher, nachher?
    t_sample sample = peek(x->buffersamps, x->buffer_size, peek_point);
    return sample;
}

/*
 MULTIPLE STREAMS
 als trigger-eingang multikanal-eingang
 anzahl der mc-channels gibt anzahl der streams (mit cap)
 danach kann man die clock parallel laufen lassen
 vielleicht auch einen mc-kanal pro stream?
 */

void ec2_perform64(t_ec2 *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam){
    t_atom_long total_streams = x->total_streams;
    t_atom_long total_voices = x->total_voices;
    t_atom_long active_streams = x->active_streams;
    active_streams = CLAMP(x->input_count, 1, total_streams);
    
    t_sample *p_trig[active_streams];
    for(int i=0;i<active_streams;i++){
        p_trig[i] = ins[i];
    }
    
    t_sample *p_playback_rate   = ins[active_streams+0];
    t_sample *p_scan_begin      = ins[active_streams+1];
    t_sample *p_scan_range      = ins[active_streams+2];
    t_sample *p_scan_speed      = ins[active_streams+3];
    t_sample *p_grain_duration  = ins[active_streams+4];
    t_sample *p_envelope_shape  = ins[active_streams+5];
    t_sample *p_pan             = ins[active_streams+6];
    t_sample *p_amplitude       = ins[active_streams+7];

    t_sample *out_l             = outs[0];
    t_sample *out_r             = outs[1];
    t_sample *debug1            = outs[2];
    t_sample *debug2            = outs[3];
    t_sample *debug3            = outs[4];
    t_sample *debug4            = outs[5];

    t_sample *mc_outs[total_voices];
    for(int i=0;i<total_voices;i++){
        mc_outs[i] = outs[i+6];
    }

    if(x->no_buffer){
        goto zero;
    }

    if(x->buffer_modified){
        ec2_buffer_limits(x);
        x->buffer_modified = FALSE;
    }

    long n=sampleframes;
    t_atom_long buffer_size = x->buffer_size;
    t_atom_long window_size = x->window_size;
    t_float samplerate      = x->samplerate;
    short count[9];
    sysmem_copyptr(x->count, count, 9*sizeof(short));
    
    //we'll have to handle amount of channels later
    //multiply the index by the amount of channels that there are
 
    while(n--){
        t_sample trig_arr[active_streams];
        t_sample playback_rate, scan_begin, scan_range, scan_speed, grain_duration, envelope_shape, pan, amplitude;
        t_sample scan_end, scan_dur, starting_point, scan_count, window_increment;

        //increment all pointers, get all values, if not connected, assign defaults
        for(int i=0;i<active_streams;i++){
            trig_arr[i] = *(p_trig[i])++;
        }
        playback_rate   = *p_playback_rate++;   playback_rate   = (x->count[1])?playback_rate:1.;
        scan_begin      = *p_scan_begin++;      scan_begin      = (x->count[2])?scan_begin:0.;
        scan_range      = *p_scan_range++;      scan_range      = (x->count[3])?scan_range:1.;
        scan_speed      = *p_scan_speed++;      scan_speed      = (x->count[4])?scan_speed:1.;
        grain_duration  = *p_grain_duration++;  grain_duration  = (x->count[5])?grain_duration:100;
        envelope_shape  = *p_envelope_shape++;  envelope_shape  = (x->count[6])?envelope_shape:0.5;
        pan             = *p_pan++;             pan             = (x->count[7])?pan:0.;
        amplitude       = *p_amplitude++;       amplitude       = (x->count[8])?amplitude:1.;
        
        //COUNTER
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
        
        for(int current_stream=0;current_stream<total_streams;current_stream++){
            t_stream *stream = &x->streams[current_stream];
            t_atom_long stream_new_index = 0;

            if(trig_arr[current_stream]>0.){
                if(stream->active_voices<x->total_voices){
                /////put this in a function maybe?
                    stream->active_voices++;
                    for(int j=0;j<x->total_voices;j++){
                        if(stream->voices[j].is_active){
                            stream_new_index = j;
                            stream->voices[j].is_active = TRUE;
                            break;
                        }
                    }
                    
                    //got our voice, fill in the data
                    scan_dur = 0;
                    if(scan_begin>scan_range){
                        scan_dur = buffer_size - scan_begin + scan_end;
                    }else{
                        scan_dur = scan_end - scan_begin;
                    }
                    
                    starting_point = fmod(scan_count + scan_begin, buffer_size+1);
                    grain_duration *= (samplerate/1000.);
                    
                    window_increment = ((t_sample) (window_size))/grain_duration;
                    
                    stream->voices[stream_new_index].scan_begin        = scan_begin;
                    stream->voices[stream_new_index].scan_end          = scan_end;
                    stream->voices[stream_new_index].playback_rate     = playback_rate;
                    stream->voices[stream_new_index].envelope_shape    = CLAMP(envelope_shape, 0, 1);
                    stream->voices[stream_new_index].pan               = CLAMP(pan, -1, 1);
                    stream->voices[stream_new_index].amplitude         = CLAMP(amplitude, 0, 1);
                    stream->voices[stream_new_index].window_increment  = window_increment;
                    
                    stream->voices[stream_new_index].window_phase      = 0;
                    stream->voices[stream_new_index].play_phase        = starting_point;
                }/////end of function?
            }
        }
        
        /*
        t_atom_long new_index = 0;
        if(trig>0.){
            //VOICE ALLOCATION
            if(x->active_voices<x->total_voices){
                x->active_voices++;
                for(int i=0;i<x->total_voices;i++){
                    if(x->voices[i].is_active==FALSE){
                        new_index = i;
                        x->voices[i].is_active = TRUE;
                        break;
                    }
                }

                //got our voice, fill in the data
                scan_dur = 0;
                if(scan_begin>scan_range){
                    scan_dur = x->buffer_size - scan_begin + scan_end;
                }else{
                    scan_dur = scan_end - scan_begin;
                }

                starting_point = fmod(scan_count + scan_begin, x->buffer_size+1);
                grain_duration *= (x->samplerate/1000.);

                t_sample window_size = x->window_size;
                window_increment = ((t_sample) (window_size))/grain_duration;

                x->voices[new_index].scan_begin = scan_begin;
                x->voices[new_index].scan_end = scan_end;
                x->voices[new_index].playback_rate = playback_rate;
                x->voices[new_index].envelope_shape = CLAMP(envelope_shape, 0, 1);
                x->voices[new_index].pan = CLAMP(pan, -1, 1);
                x->voices[new_index].amplitude = CLAMP(amplitude, 0, 1);
                x->voices[new_index].window_increment = window_increment;

                x->voices[new_index].window_phase = 0;
                x->voices[new_index].play_phase = starting_point;
            }
        }
        */
        
        //MC STREAMS PLAYBACK
        //cache total_streams;
        t_sample accum_l[x->total_streams];
        t_sample accum_r[x->total_streams];
        t_sample w_accum[x->total_streams];
        /*
        //PLAYBACK
        t_sample accum_l = 0;
        t_sample accum_r = 0;

        t_sample w_accum = 0;
        for(int i=0;i<x->total_voices;i++){
            if(x->voices[i].is_active == TRUE){
                t_sample windowsamp = window(x, i);
                w_accum += windowsamp;

                t_sample playbacksamp = playback(x, i);
                playbacksamp *= windowsamp;
                playbacksamp *= x->voices[i].amplitude;
                playbacksamp *= (t_sample) 1./x->total_voices;
                //normalizing for now until I come up with something smarter
                accum += playbacksamp;

                t_sample pan_l, pan_r;
                cospan(playbacksamp, x->voices[i].pan, &pan_l, &pan_r);
                accum_l += pan_l;
                accum_r += pan_r;
            }
        }
        
        *out_l++ = FIX_DENORM_NAN_SAMPLE(accum_l);
        *out_r++ = FIX_DENORM_NAN_SAMPLE(accum_r);

        //single channel output of active voices
        *debug1++ = x->active_voices;
        *debug2++ = (t_sample)(mfmod(scan_count + scan_begin, x->buffer_size+1))/x->buffer_size;
        *debug3++ = (t_sample)scan_begin/x->buffer_size;
        *debug4++ = (t_sample)scan_end/x->buffer_size;

        for(int i=0;i<x->total_voices;i++){
            *mc_outs[i]++ = x->voices[i].is_active;
        }
        */
        //reassign cached values that update sample wise HERE:
        x->scan_count = scan_count;
    }
    
    //reassign cached values that update in block size HERE:
    x->active_streams = active_streams;
    return;
zero:
    for(int i=0;i<numouts;i++){
        set_zero64(outs[i], sampleframes);
    }
}

/***HOUSEKEEPING***/

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

    if(x->streams){
        for(int i=0;i<x->total_streams;i++){
            sysmem_freeptr(x->streams[i].voices);
        }
    }
    sysmem_freeptr(x->streams);
    
    if(x->buffersamps){
        sysmem_freeptr(x->buffersamps);
    }
}

void ec2_assist(t_ec2 *x, void *b, long m, long a, char *s){
    if(m == ASSIST_INLET){
        switch(a){
            case 0:
                sprintf(s, "(mcsignal) Trigger");
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
                sprintf(s, "(signal) Left output");
                break;
            case 1:
                sprintf(s, "(signal) Right output");
                break;
        }
    }
}

void voice_and_param(t_ec2 *x, t_sample ***ins_p){
    /*
    //use this function when we're sure that everything else is working maybe
    //get a single sample and increment the pointer ayyyy
    t_sample trig           = *(*((*ins_p)+0))++;
    t_sample playback_rate  = *(*((*ins_p)+1))++;
    t_sample scan_begin     = *(*((*ins_p)+2))++;
    t_sample scan_range     = *(*((*ins_p)+3))++;
    t_sample scan_speed     = *(*((*ins_p)+4))++;
    t_sample grain_duration = *(*((*ins_p)+5))++;
    t_sample envelope_shape = *(*((*ins_p)+6))++;
    t_sample pan            = *(*((*ins_p)+7))++;
    t_sample amplitude      = *(*((*ins_p)+8))++;
     */
}
