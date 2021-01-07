#include "ec2~.h"
#include "gen.h"
#include "mydsp.h"

void ext_main(void *r){
    t_class *c;
    
    c = class_new("ec2~", (method)ec2_new, (method)ec2_free, sizeof(t_ec2), NULL, A_GIMME, 0);
    class_addmethod(c, (method)ec2_dsp64,     "dsp64",    A_CANT, 0);
    class_addmethod(c, (method)ec2_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)ec2_dblclick, "dblclick", A_CANT, 0);
    class_addmethod(c, (method)ec2_notify, "notify", A_CANT, 0);
    class_addmethod(c, (method)ec2_set, "set", A_SYM, 0);
    class_addmethod(c, (method)ec2_multichanneloutputs, "multichanneloutputs", A_CANT, 0);

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
    x->buffer = NULL;
    x->buffer_modified = TRUE;
    x->buffer_size = 1;
    
    t_symbol *bufname = atom_getsymarg(0, argc, argv);
    ec2_set(x, bufname);
    return (x);
}

t_sample window(t_ec2 *x, t_atom_long voice_index){
    //side effects: increases window_phase, changes is_active (when done), active_voices is decreased
    //window determines the "life time" of a single grain!
    t_sample window_phase = x->voices[voice_index].window_phase;
    window_phase += x->voices[voice_index].window_increment;
    x->voices[voice_index].window_phase = window_phase;
    
    if(window_phase>=x->window_size){
        x->voices[voice_index].is_active = FALSE;
        x->active_voices--;
        return 0;
    }
    
    t_sample tuk        = peek(x->tukey, x->window_size, window_phase);
    t_sample expo       = peek(x->expodec, x->window_size, window_phase);
    t_sample rexpo      = peek(x->rexpodec, x->window_size, window_phase);
    t_sample env_shape  = x->voices[voice_index].envelope_shape;
    
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

t_sample playback(t_ec2 *x, t_atom_long voice_index, t_sample *buf){
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
    t_sample sample = peek(buf, x->buffer_size, peek_point);
    return sample;
}

//OPTIMIZATION THOUGHTS
//apparently it's faster if there are no functions in for loops, so check that out

//t_sample mfmod(t_sample x, t_sample y){double a;a=x/y;a-=(int)a; return a*y;}
//as a replacement for fmod?
//or comparison if over threshold and then reset?

//calculating expo and tukey instead of LUT?

//fast cos in panning function? maybe not as gen~ itself doesn't do that in the exported cospan.gendsp

//assign defaults at block rate?


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

    t_sample *out_l             = outs[0];
    t_sample *out_r             = outs[1];
    t_sample *debug1            = outs[2];
    t_sample *debug2            = outs[3];
    t_sample *debug3            = outs[4];
    t_sample *debug4            = outs[5];
    
    t_sample *mc_outs[x->total_voices];
    for(int i=0;i<x->total_voices;i++){
        mc_outs[i] = outs[i+6];
    }
    
    t_buffer_obj *bref = buffer_ref_getobject(x->l_buffer_reference);
    t_float *buffersamps = buffer_locksamples(bref);
    if(!bref || !buffersamps){
        goto zero;
    }
   
    if(x->buffer_modified){
        x->buffer_size      = buffer_getframecount(bref)-1;
        x->channel_count    = buffer_getchannelcount(bref);
        
        if(x->buffer){
            sysmem_freeptr(x->buffer);
        }
        
        x->buffer = (t_sample *)sysmem_newptr(x->buffer_size * sizeof(t_sample));
        
        for(long i=0;i<x->buffer_size;i++){
            x->buffer[i] = (t_sample)buffersamps[i];
        }
        x->buffer_modified = FALSE;
    }
    
    long n=sampleframes;
    //we'll have to handle amount of channels later
    //multiply the index by the amount of channels that there are
 

    while(n--){
        t_sample trig, playback_rate, scan_begin, scan_range, scan_speed, grain_duration, envelope_shape, pan, amplitude;
        t_sample scan_end, scan_dur, starting_point, scan_count, window_increment;

        //increment all pointers, get all values, if not connected, assign defaults
        trig            = *p_trig++;
        playback_rate   = *p_playback_rate++;   playback_rate   = (x->count[1])?playback_rate:1.;
        scan_begin      = *p_scan_begin++;      scan_begin      = (x->count[2])?scan_begin:0.;
        scan_range      = *p_scan_range++;      scan_range      = (x->count[3])?scan_range:1.;
        scan_speed      = *p_scan_speed++;      scan_speed      = (x->count[4])?scan_speed:1.;
        grain_duration  = *p_grain_duration++;  grain_duration = (x->count[5])?grain_duration:100;
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
        
        scan_begin = CLAMP(scan_begin, 0, 1)*x->buffer_size;
        scan_range = CLAMP(scan_range, 0, 1)*x->buffer_size;
        
        //overflow
        scan_count = fmod(scan_count, scan_range+1);
        //scan_count = (scan_count>=scan_range)?0:scan_count;
        //underflow
        scan_count = (scan_count<0)?scan_range:scan_count;
        x->scan_count = scan_count;
        scan_end = fmod(scan_range + scan_begin, x->buffer_size+1);

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
        //PLAYBACK
        t_sample accum_l = 0;
        t_sample accum_r = 0;
        t_sample accum = 0;
        
        t_sample w_accum = 0;
        for(int i=0;i<x->total_voices;i++){
            if(x->voices[i].is_active == TRUE){
                t_sample windowsamp = window(x, i);
                w_accum += windowsamp;
                
                t_sample playbacksamp = playback(x, i, x->buffer);
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
        *debug2++ = (t_sample)(fmod(scan_count + scan_begin, x->buffer_size+1))/x->buffer_size;
        *debug3++ = (t_sample)scan_begin/x->buffer_size;
        *debug4++ = (t_sample)scan_end/x->buffer_size;

        for(int i=0;i<x->total_voices;i++){
         *mc_outs[i]++ = x->voices[i].is_active;
        }
    }
    buffer_unlocksamples(bref);
    return;
zero:
    for(int i=0;i<numouts;i++){
        set_zero64(outs[i], sampleframes);
    }
}

/***HOUSEKEEPING***/

void ec2_free(t_ec2 *x){
    dsp_free((t_pxobject *)x);
    object_free(x->l_buffer_reference);
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
    
    if(x->buffer){
        sysmem_freeptr(x->buffer);
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
