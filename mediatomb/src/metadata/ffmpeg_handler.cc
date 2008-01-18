/*
    MediaTomb - http://www.mediatomb.cc/
    
    ffmpeg_handler.cc - this file is part of MediaTomb.
    
    Copyright (C) 2005 Gena Batyan <bgeradz@mediatomb.cc>,
                       Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>
    
    Copyright (C) 2006-2007 Gena Batyan <bgeradz@mediatomb.cc>,
                            Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>,
                            Leonhard Wimmer <leo@mediatomb.cc>

    Copyright (C) 2007 Ingo Preiml <ipreiml@edu.uni-klu.ac.at>
    
    MediaTomb is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.
    
    MediaTomb is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    version 2 along with MediaTomb; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
    
    $Id$
*/

/// \file ffmpeg_handler.cc
/// \brief Implementeation of the FfmpegHandler class.

// Information about the stream are to be found in the structure
// AVFormatContext, defined in libavformat/avformat.h:335
// and in the structure
// AVCodecContext, defined in libavcodec/avcodec.h:722
// in the ffmpeg sources

#ifdef HAVE_CONFIG_H
    #include "autoconfig.h"
#endif

#ifdef HAVE_FFMPEG

// ffmpeg needs the following sources
// INT64_C is not defined in ffmpeg/avformat.h but is needed
#define __STDC_CONSTANT_MACROS
#include <stdint.h>

#ifdef FFMPEG_NEEDS_EXTERN_C
extern "C" 
{
#endif

#include <ffmpeg/avformat.h>

#ifdef FFMPEG_NEEDS_EXTERN_C
} // extern "C"
#endif

#include "config_manager.h"
#include "ffmpeg_handler.h"
#include "string_converter.h"
#include "common.h"
#include "tools.h"
#include "rexp.h"
#include "mxml/mxml.h"

using namespace zmm;
using namespace mxml;

// Default constructor
FfmpegHandler::FfmpegHandler() : MetadataHandler()
{
}

static void addFfmpegMetadataFields(Ref<CdsItem> item, AVFormatContext *pFormatCtx) 
{

	Ref<StringConverter> sc = StringConverter::m2i();
    
	if (strlen(pFormatCtx->title) > 0) 
    {
	    log_debug("Added metadata title: %s\n", pFormatCtx->title);
        item->setMetadata(String(MT_KEYS[M_TITLE].upnp), 
                          sc->convert(String(pFormatCtx->title)));
	}
	if (strlen(pFormatCtx->author) > 0) 
    {
	    log_debug("Added metadata author: %s\n", pFormatCtx->author);
        item->setMetadata(String(MT_KEYS[M_ARTIST].upnp), 
                          sc->convert(String(pFormatCtx->author)));
	}
	if (strlen(pFormatCtx->album) > 0) 
    {
	    log_debug("Added metadata album: %s\n", pFormatCtx->album);
        item->setMetadata(String(MT_KEYS[M_ALBUM].upnp), 
                          sc->convert(String(pFormatCtx->album)));
	}
	if (pFormatCtx->year > 0) 
    {
	    log_debug("Added metadata year: %d\n", pFormatCtx->year);
        item->setMetadata(String(MT_KEYS[M_DATE].upnp), 
                          sc->convert(String::from(pFormatCtx->year)));
	}
	if (strlen(pFormatCtx->genre) > 0) 
    {
	    log_debug("Added metadata genre: %s\n", pFormatCtx->genre);
        item->setMetadata(String(MT_KEYS[M_GENRE].upnp), 
                          sc->convert(String(pFormatCtx->genre)));
	}
	if (strlen(pFormatCtx->comment) > 0) 
    {
	    log_debug("Added metadata comment: %s\n", pFormatCtx->comment);
        item->setMetadata(String(MT_KEYS[M_DESCRIPTION].upnp), 
                          sc->convert(String(pFormatCtx->comment)));
	}
	if (pFormatCtx->track > 0) 
    {
	    log_debug("Added metadata track: %d\n", pFormatCtx->track);
        item->setMetadata(String(MT_KEYS[M_TRACKNUMBER].upnp), 
                          sc->convert(String::from(pFormatCtx->track)));
	}
}

// ffmpeg library calls
static void addFfmpegResourceFields(Ref<CdsItem> item, AVFormatContext *pFormatCtx) 
{
    unsigned int i;
    int hours, mins, secs, us;
    int audioch = 0, samplefreq = 0;
    bool audioset, videoset;
    char duration[15], resolution[25];

    // Initialize the buffers
    duration[0] = 0;
    resolution[0] = 0; 

	// duration
    secs = pFormatCtx->duration / AV_TIME_BASE;
    us = pFormatCtx->duration % AV_TIME_BASE;
    mins = secs / 60;
    secs %= 60;
    hours = mins / 60;
    mins %= 60;
    if (hours > 0 && mins > 0 && secs > 0) 
    { 
    	sprintf(duration, "%02d:%02d:%02d.%01d", hours, mins,
                secs, (10 * us) / AV_TIME_BASE);
        log_debug("Added duration: %s\n", duration);
        item->getResource(0)->addAttribute(MetadataHandler::getResAttrName(R_DURATION), String(duration));
    }

	// bitrate
    if (pFormatCtx->bit_rate > 0)  
    {
        log_debug("Added overall bitrate: %d kb/s\n", 
                  pFormatCtx->bit_rate/1000);
        item->getResource(0)->addAttribute(MetadataHandler::getResAttrName(R_BITRATE), String::from(pFormatCtx->bit_rate/1000));
    }

	// video resolution, audio sampling rate, nr of audio channels
	audioset = false;
	videoset = false;
	for(i=0; i<pFormatCtx->nb_streams; i++) 
    {
		AVStream *st = pFormatCtx->streams[i];
		if((videoset == false) && (st->codec->codec_type == CODEC_TYPE_VIDEO))
        {
			if ((st->codec->width > 0) && (st->codec->height > 0)) 
            {
				sprintf(resolution, "%dx%d", st->codec->width, st->codec->height);
				log_debug("Added resolution: %s pixel\n", resolution);
		        item->getResource(0)->addAttribute(MetadataHandler::getResAttrName(R_RESOLUTION), String(resolution));
				videoset = true;
			}
		} 
		if(st->codec->codec_type == CODEC_TYPE_AUDIO) 
        {
			// Increase number of audiochannels
			audioch++;
			// Get the sample rate
			if ((audioset == false) && (st->codec->sample_rate > 0)) 
            {
				samplefreq = st->codec->sample_rate;
			    if (samplefreq > 0) 
                {
		    	    log_debug("Added sample frequency: %d Hz\n", samplefreq);
		        	item->getResource(0)->addAttribute(MetadataHandler::getResAttrName(R_SAMPLEFREQUENCY), String::from(samplefreq));
					audioset = true;
    			}
			}
		}
	}

    if (audioch > 0) 
    {
        log_debug("Added number of audio channels: %d\n", audioch);
        item->getResource(0)->addAttribute(MetadataHandler::getResAttrName(R_NRAUDIOCHANNELS), String::from(audioch));
    }
} // addFfmpegResourceFields

/*double time_to_double(struct timeval time) {
        return time.tv_sec + (time.tv_usec / 1000000.0);
}*/

// Stub for suppressing ffmpeg error messages during matadata extraction
void FfmpegNoOutputStub(void* ptr, int level, const char* fmt, va_list vl)
{
	// do nothing
}

void FfmpegHandler::fillMetadata(Ref<CdsItem> item)
{
	/*struct timeval start, end;
	double cumTime = 0;
	gettimeofday(&start, NULL);*/	

    log_debug("Running ffmpeg handler\n");

	AVFormatContext *pFormatCtx;
	
	// Suppress all log messages
	av_log_set_callback(FfmpegNoOutputStub);
	
	// Register all formats and codecs
    av_register_all();

    // Open video file
    if(av_open_input_file(&pFormatCtx, 
                          item->getLocation().c_str(), NULL, 0, NULL) != 0)
        return; // Couldn't open file

    // Retrieve stream information
    if(av_find_stream_info(pFormatCtx)<0)
        return; // Couldn't find stream information
        
	// Add metadata using ffmpeg library calls
	addFfmpegMetadataFields(item, pFormatCtx);
	// Add resources using ffmpeg library calls
	addFfmpegResourceFields(item, pFormatCtx);
	
    // Close the video file
    av_close_input_file(pFormatCtx);

	/*gettimeofday(&end, NULL);
	cumTime = time_to_double(end)-time_to_double(start);	
	printf("Rrun time: %lf s\n", cumTime);*/
}

Ref<IOHandler> FfmpegHandler::serveContent(Ref<CdsItem> item, int resNum, off_t *data_size)
{
    *data_size = -1;
    return nil;
}

#endif // HAVE_FFMPEG