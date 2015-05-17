#pragma comment(user, "license")

#include "FlacDecoder.h"
#include "flac/all.h"
#include "AudioDecoder.h"

using namespace nqr;

class FlacDecoderInternal
{
    
public:
    
    // N.B.: FLAC is a big-endian format. All values are unsigned.
    FlacDecoderInternal(AudioData * d, std::string filepath) : d(d)
    {
        
        /////////////////////////////
        // Initialize FLAC library //
        /////////////////////////////
        
        decoderInternal = FLAC__stream_decoder_new();
        
        FLAC__stream_decoder_set_metadata_respond(decoderInternal, FLAC__METADATA_TYPE_STREAMINFO);
        
        //@todo: check if OGG flac
        bool initialized = FLAC__stream_decoder_init_file(decoderInternal,
                                                          filepath.c_str(),
                                                          s_writeCallback,
                                                          s_metadataCallback,
                                                          s_errorCallback,
                                                          this) == FLAC__STREAM_DECODER_INIT_STATUS_OK;
        
        FLAC__stream_decoder_set_md5_checking(decoderInternal, true);
        
        //////////////////////
        // Read Stream Data //
        /////////////////////
        
        if (initialized)
        {
            // Find the size and allocate memory
            FLAC__stream_decoder_process_until_end_of_metadata(decoderInternal);
            
            // Read memory out into our temporary internalBuffer
            FLAC__stream_decoder_process_until_end_of_stream(decoderInternal);

            // Presently unneeded, but useful for reference
            // FLAC__ChannelAssignment channelAssignment = FLAC__stream_decoder_get_channel_assignment(decoderInternal);
            
            // Fill out remaining user data
            d->lengthSeconds = (float) numSamples / (float) d->sampleRate;
            
            auto totalSamples = numSamples * d->channelCount;
            
			// N.B.: "Currently the reference encoder and decoders only support up to 24 bits per sample."

            PCMFormat internalFmt = PCMFormat::PCM_END;

			switch (d->bitDepth)
			{
			case 8:
				internalFmt = PCMFormat::PCM_S8;
				break;
			case 16:
				internalFmt = PCMFormat::PCM_16;
				break;
			case 24:
				internalFmt = PCMFormat::PCM_24;
				break;
			default:
				throw std::runtime_error("unsupported FLAC bit depth");
				break;
			}

            // Next, process internal buffer into the user-visible samples array
            ConvertToFloat32(d->samples.data(), internalBuffer.data(), totalSamples, internalFmt);
        }
        
        else
        {
            throw std::runtime_error("Unable to initialize FLAC decoder");
        }
        
    }
    
    ~FlacDecoderInternal()
    {
        if (decoderInternal)
        {
            FLAC__stream_decoder_finish(decoderInternal);
            FLAC__stream_decoder_delete(decoderInternal);
        }
    }
    
    void processMetadata(const FLAC__StreamMetadata_StreamInfo & info)
    {
        d->sampleRate = info.sample_rate;
        d->channelCount = info.channels; // Assert 1 to 8
        d->bitDepth = info.bits_per_sample; // Assert 4 to 32
        d->frameSize = info.channels * info.bits_per_sample;
        
        const size_t bytesPerSample = d->bitDepth / 8;

        numSamples = (size_t) info.total_samples;
        
        internalBuffer.resize(numSamples * info.channels * bytesPerSample); // as array of bytes
        d->samples.resize(numSamples * info.channels); // as audio samples in float32
    }

    ///////////////////////
    // libflab callbacks //
    ///////////////////////
    
    static FLAC__StreamDecoderWriteStatus s_writeCallback(const FLAC__StreamDecoder*, const FLAC__Frame* frame, const FLAC__int32* const buffer[], void* userPtr)
    {
        FlacDecoderInternal * decoder = reinterpret_cast<FlacDecoderInternal *>(userPtr);
        
        const size_t bytesPerSample = decoder->d->bitDepth / 8;
        
        auto dataPtr = decoder->internalBuffer.data();
        
        for(unsigned int i = 0;  i < frame->header.blocksize; i++)
        {
            for(int j = 0; j < decoder->d->channelCount; j++)
            {
                memcpy(dataPtr + decoder->bufferPosition, &buffer[j][i], bytesPerSample);
                decoder->bufferPosition += bytesPerSample;
            }
        }
        
        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    }
    
    static void s_metadataCallback (const FLAC__StreamDecoder*, const FLAC__StreamMetadata* metadata, void* userPtr)
    {
        static_cast<FlacDecoderInternal*>(userPtr)->processMetadata(metadata->data.stream_info);
    }
    
    static void s_errorCallback (const FLAC__StreamDecoder *, FLAC__StreamDecoderErrorStatus status, void*)
    {
        std::cerr << "FLAC Decoder Error: " << FLAC__StreamDecoderErrorStatusString[status] << std::endl;
    }
    
private:
    
    NO_COPY(FlacDecoderInternal);
    
    FLAC__StreamDecoder * decoderInternal;
    
    size_t bufferPosition = 0;
    size_t numSamples = 0;
    
    AudioData * d;
    
    std::vector<uint8_t> internalBuffer;
};

//////////////////////
// Public Interface //
//////////////////////

int FlacDecoder::LoadFromPath(AudioData * data, const std::string & path)
{
    FlacDecoderInternal decoder(data, path);
    return IOError::NoError;
}

int FlacDecoder::LoadFromBuffer(AudioData * data, const std::vector<uint8_t> & memory)
{
    return IOError::LoadBufferNotImplemented;
}

std::vector<std::string> FlacDecoder::GetSupportedFileExtensions()
{
    return {"flac"};
}