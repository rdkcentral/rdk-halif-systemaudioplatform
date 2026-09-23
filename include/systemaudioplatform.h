/**
*  If not stated otherwise in this file or this component's LICENSE
*  file the following copyright and licenses apply:
*
*  Copyright 2026 RDK Management
*
*  Licensed under the Apache License, Version 2.0 (the License);
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*  http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an AS IS BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
*/

#ifndef _SYSTEMAUDIOPLATFORM_H_
#define _SYSTEMAUDIOPLATFORM_H_

#include <stdint.h>
#include <string>
#include <gst/gst.h>
#include <gst/audio/audio.h>

/**
*  @file      systemaudioplatform.h
*  @brief     System Audio Platform (SAP) SoC abstraction interface.
*
*  The System Audio Platform (SAP) interface provides an abstraction between the SoC-agnostic RDK
*  `SystemAudioPlayer` service and the underlying SoC specific audio implementation. 
*  It allows system/notification/TTS sounds to be rendered and mixed over primary media that is already playing.
*
*/

/**
*  @brief Encoding of the audio content to be played.
*/
   enum AudioType
   {
       AudioType_None,  /**< Unspecified / invalid audio type. */
       PCM,             /**< Raw PCM audio. */
       MP3,             /**< MP3 encoded audio. */
       WAV              /**< WAV container audio. */
   };

/**
*  @brief Origin of the audio data feeding the pipeline source.
*/
   enum SourceType
   {
       SourceType_None, /**< Unspecified / invalid source type. */
       DATA,            /**< Raw in-memory application data buffer. */
       HTTPSRC,         /**< Audio streamed over HTTP. */
       FILESRC,         /**< Audio read from a local file. */
       WEBSOCKET        /**< Audio streamed over a websocket. */
   };

/**
*  @brief Playback routing mode which selects how the sound is mixed.
*/
   enum PlayMode
   {
       PlayMode_None,   /**< Unspecified / invalid play mode. */
       SYSTEM,          /**< System mode (mixed with primary audio). */
       APP              /**< Application/TTS mode. */
   };

/**
*  @brief Selects which hardware mixer gain is adjusted.
*/
   enum MixGain {
        MIXGAIN_PRIM,   /**< Primary (main media) mix gain. */
        MIXGAIN_SYS,    /**< System mix gain. */
        MIXGAIN_TTS     /**< App/TTS mix gain. */
   };

    /**
     * Initializes the System Audio Platform.
     *
     * Loads the audio HAL interface so that hardware mix gain can be
     * controlled. Must be called once before any playback or volume control.
     * Safe to call when already initialized (no-op).
     *
     * @see systemAudioDeinitialize()
     */
    void systemAudioInitialize();

    /**
     * Deinitializes the System Audio Platform.
     *
     * Unloads the audio HAL interface previously loaded by
     * systemAudioInitialize() and releases associated resources.
     *
     * @see systemAudioInitialize()
     */
    void systemAudioDeinitialize();

    /**
     * Changes the hardware mixer gain for a given stream.
     *
     * The linear @p volume (0-100) is converted to a dB value and applied to the
     * selected mixer via the audio HAL.
     *
     * @param[in] gain      The mixer whose gain is to be changed.
     * @param[in] volume    Linear volume level in the (range 0 to 100).
     *
     * @pre systemAudioInitialize() must have been called successfully.
     */
    void systemAudioChangePrimaryVol(MixGain gain,int volume);

    /**
     * Sets the smart-volume silence detection window (detect time).
     *
     * Configures the GStreamer `cutter` element run-length used to detect
     * silence before muting/ducking.
     *
     * @param[in] detectTimeMs   Detection time in milliseconds.
     *
     * @pre The pipeline must have been created with smartVolumeEnable = true.
     * @see systemAudioGeneratePipeline(), systemAudioSetHoldTime()
     */
    void systemAudioSetDetectTime( int detectTimeMs);

    /**
     * Sets the smart-volume hold time.
     *
     * Configures how long the GStreamer `cutter` element holds its state before
     * transitioning.
     *
     * @param[in] holdTimeMs   Hold time in milliseconds.
     *
     * @pre The pipeline must have been created with smartVolumeEnable = true.
     * @see systemAudioGeneratePipeline(), systemAudioSetDetectTime()
     */
    void systemAudioSetHoldTime( int holdTimeMs);

    /**
     * Sets the smart-volume detection threshold.
     *
     * Configures the amplitude threshold of the GStreamer `cutter` element used
     * to distinguish silence from audio.
     *
     * @param[in] thresHold   The threshold value applied to the cutter element.
     *
     * @pre The pipeline must have been created with smartVolumeEnable = true.
     * @see systemAudioGeneratePipeline()
     */
    void systemAudioSetThreshold(double thresHold);

    /**
     * Sets the playback volume for the current stream.
     *
     * For PCM and WAV content the volume is applied through the hardware
     * mixer gain (system or app, based on @p playMode). For MP3 content the
     * volume is applied via the sink's `stream-volume` property.
     *
     * @param[in] audioVolume   The GStreamer element used to control volume (as returned by systemAudioGeneratePipeline()).
     * @param[in] audioType     The audio encoding being played.
     * @param[in] playMode      The routing mode (SYSTEM or APP).
     * @param[in] thisVol       Linear volume level in the range 0 to 100.
     *
     * @pre systemAudioInitialize() must have been called successfully.
     */
    void systemAudioSetVolume(GstElement *audioVolume,AudioType audioType,PlayMode playMode,int thisVol);

    /**
     * Builds the GStreamer pipeline for the requested audio.
     *
     * Creates and links the decode/convert/resample elements appropriate for the
     * given @p type and @p sourceType, terminating in the SoC audio sink.
     * When @p smartVolumeEnable is true a silence-detection (cutter) element is
     * inserted. The created sink and volume-control elements are returned via
     * @p audioSink and @p audioVolume.
     *
     * @param[in]  pipeline           The GStreamer pipeline (bin) to populate.
     * @param[in]  source             The source element feeding the pipeline.
     * @param[in]  capsfilter         Caps filter used for PCM file/http sources (may be unused for DATA/WEBSOCKET).
     * @param[in]  type               The audio encoding to play (PCM, MP3, WAV).
     * @param[in]  mode               The playback routing mode (SYSTEM or APP).
     * @param[in]  sourceType         The origin of the audio data.
     * @param[in]  smartVolumeEnable  true to insert the smart-volume cutter element.
     * @param[out] audioSink          Receives the created audio sink element.
     * @param[out] audioVolume        Receives the element used for volume control
     *
     * @returns boolean
     * @retval true  - the pipeline elements were created and linked successfully.
     * @retval false - one or more elements could not be created or linked.
     *
     * @pre systemAudioInitialize() must have been called successfully.
     * @note The caller retains ownership of pipeline.
     * @note source, audioSink and audioVolume are owned by the pipeline once added to the bin 
     * and must not be unreferenced independently
     */
    bool systemAudioGeneratePipeline(GstElement  *pipeline, GstElement  *source,GstElement *capsfilter,GstElement **audioSink,GstElement **audioVolume,AudioType type,PlayMode mode,SourceType sourceType,bool smartVolumeEnable);
#endif // _SYSTEMAUDIOPLATFORM_H
 