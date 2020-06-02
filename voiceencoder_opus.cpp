#include "ivoicecodec.h"
#include "iframeencoder.h"

#ifdef _WIN32
#include <win32/config.h>
#endif
#include <stdio.h>
#include <opus.h>
#include <opus_custom.h>
//#include <silk/API.h>
#include <tier0\dbg.h>

#include "tier0/memdbgon.h"


#define CHANNELS 1

struct opus_versions
{
	int iSampleRate;
	int iRawFrameSize;
	int iPacketSize;
};

#define OPUS_VERSION 4

opus_versions g_OpusVersion[OPUS_VERSION]
{
	{
		44100, 256, 120
	},

	{
		22050, 120, 60
	},

	{
		22050, 256, 60
	},

	{
		22050, 512, 64
	},
};

class VoiceEncoder_Opus : public IFrameEncoder
{
public:
	VoiceEncoder_Opus();
	virtual ~VoiceEncoder_Opus();

	bool Init(int quality, int& rawFrameSize, int& encodedFrameSize);
	void Release();
	void DecodeFrame(const char* pCompressed, char* pDecompressedBytes);
	void EncodeFrame(const char* pUncompressedBytes, char* pCompressed);
	bool ResetState();

private:
	bool	InitStates();
	void	TermStates();

	OpusCustomEncoder* m_EncoderState;
	OpusCustomDecoder* m_DecoderState;
	OpusCustomMode* m_Mode;

	int m_iVersion;
};

extern IVoiceCodec* CreateVoiceCodec_Frame(IFrameEncoder* pEncoder);

void* CreateOpusVoiceCodec()
{
	IFrameEncoder* pEncoder = new VoiceEncoder_Opus;
	return CreateVoiceCodec_Frame(pEncoder);
}

EXPOSE_INTERFACE_FN(CreateOpusVoiceCodec, IVoiceCodec, "vaudio_opus");

VoiceEncoder_Opus::VoiceEncoder_Opus()
{
	m_EncoderState = NULL;
	m_DecoderState = NULL;
	m_Mode = NULL;
	m_iVersion = 0;
}

VoiceEncoder_Opus::~VoiceEncoder_Opus()
{
	TermStates();
}

bool VoiceEncoder_Opus::Init(int quality, int& rawFrameSize, int& encodedFrameSize)
{
	m_iVersion = quality;

	rawFrameSize = g_OpusVersion[m_iVersion].iRawFrameSize * BYTES_PER_SAMPLE;

	int iError = 0;

	m_Mode = opus_custom_mode_create(g_OpusVersion[m_iVersion].iSampleRate, g_OpusVersion[m_iVersion].iRawFrameSize, &iError);
	m_EncoderState = opus_custom_encoder_create(m_Mode, CHANNELS, NULL);
	m_DecoderState = opus_custom_decoder_create(m_Mode, CHANNELS, NULL);

	if (!InitStates())
		return false;

	encodedFrameSize = g_OpusVersion[m_iVersion].iPacketSize;
	
	return true;
}

void VoiceEncoder_Opus::Release()
{
	delete this;
}

void VoiceEncoder_Opus::EncodeFrame(const char* pUncompressedBytes, char* pCompressed)
{
	unsigned char output[1024];

	opus_custom_encode(m_EncoderState, (opus_int16*)pUncompressedBytes, g_OpusVersion[m_iVersion].iRawFrameSize, output, g_OpusVersion[m_iVersion].iPacketSize);

	for (int i = 0; i < g_OpusVersion[m_iVersion].iPacketSize; i++)
	{
		*pCompressed = (char)output[i];
		pCompressed++;
	}
}

void VoiceEncoder_Opus::DecodeFrame(const char* pCompressed, char* pDecompressedBytes)
{
	unsigned char output[1024];
	char* out = (char*)pCompressed;

	if (!pCompressed)
	{
		opus_custom_decode(m_DecoderState, NULL, g_OpusVersion[m_iVersion].iPacketSize, (opus_int16*)pDecompressedBytes, g_OpusVersion[m_iVersion].iRawFrameSize);
	}

	for (int i = 0; i < g_OpusVersion[m_iVersion].iPacketSize; i++)
	{
		output[i] = (unsigned char)((*out < 0) ? (*out + 256) : *out);
		out++;
	}

	opus_custom_decode(m_DecoderState, output, g_OpusVersion[m_iVersion].iPacketSize, (opus_int16*)pDecompressedBytes, g_OpusVersion[m_iVersion].iRawFrameSize);
}

bool VoiceEncoder_Opus::ResetState()
{
	opus_custom_encoder_ctl(m_EncoderState, OPUS_RESET_STATE, NULL);
	opus_custom_decoder_ctl(m_DecoderState, OPUS_RESET_STATE, NULL);

	return true;
}

bool VoiceEncoder_Opus::InitStates()
{
	if (!m_EncoderState || !m_DecoderState)
		return false;

	opus_custom_encoder_ctl(m_EncoderState, OPUS_RESET_STATE, NULL);
	opus_custom_decoder_ctl(m_DecoderState, OPUS_RESET_STATE, NULL);

	return true;
}

void VoiceEncoder_Opus::TermStates()
{
	if (m_EncoderState)
	{
		opus_custom_encoder_destroy(m_EncoderState);
		m_EncoderState = NULL;
	}

	if (m_DecoderState)
	{
		opus_custom_decoder_destroy(m_DecoderState);
		m_DecoderState = NULL;
	}

	opus_custom_mode_destroy(m_Mode);
}