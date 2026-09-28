#define NOMINMAX
#include "sound.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <algorithm>
#include <mferror.h>
#include <string>
#include <vector>
#include <cmath>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#ifdef _XBOX //Big-Endian
#define fourccRIFF 'RIFF'
#define fourccDATA 'data'
#define fourccFMT 'fmt '
#define fourccWAVE 'WAVE'
#define fourccXWMA 'XWMA'
#define fourccDPDS 'dpds'
#endif
#ifndef _XBOX //Little-Endian
#define fourccRIFF 'FFIR'
#define fourccDATA 'atad'
#define fourccFMT ' tmf'
#define fourccWAVE 'EVAW'
#define fourccXWMA 'AMWX'
#define fourccDPDS 'sdpd'

#endif

//=============================================================================
// 初期化
//=============================================================================
HRESULT Sound::Init()
{
	HRESULT hr;

	HANDLE hFile;
	DWORD  dwChunkSize;
	DWORD  dwChunkPosition;
	DWORD  filetype;

	// COMの初期化
	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr)) {
		CoUninitialize();
		return -1;
	}

	//Media Foundationの初期か
	hr = MFStartup(MF_VERSION);
	if (FAILED(hr))
	{
		CoUninitialize();
		return -1;
	}

	/**** Create XAudio2 ****/
	hr = XAudio2Create(&m_pXAudio2, 0);		// 第二引数は､動作フラグ デバッグモードの指定(現在は未使用なので0にする)
	//hr=XAudio2Create(&g_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);		// 第三引数は、windowsでは無視
	if (FAILED(hr)) {
		CoUninitialize();
		return -1;
	}

	/**** Create Mastering Voice ****/
	hr = m_pXAudio2->CreateMasteringVoice(&m_pMasteringVoice);			// 今回はＰＣのデフォルト設定に任せている
	/*, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, NULL*/		// 本当６個の引数を持っている
	if (FAILED(hr)) {
		if (m_pXAudio2)	m_pXAudio2->Release();
		CoUninitialize();
		return -1;
	}

	/**** Initalize Sound ****/
	for (int i = 0; i < SOUND_LABEL_MAX; i++)
	{
		memset(&m_wfx[i], 0, sizeof(WAVEFORMATEXTENSIBLE));
		memset(&m_buffer[i], 0, sizeof(XAUDIO2_BUFFER));

		std::string path = m_param[i].filename;
		std::string ext = path.substr(path.find_last_of(".") + 1);
		for (auto& c : ext) c = (char)tolower((unsigned char)c);

		bool loaded = false;

		if (ext == "mp3")
		{
			DWORD dataSize = 0;
			HRESULT hrLocal = LoadMp3AsPCM(m_param[i].filename, m_wfx[i], m_DataBuffer[i], dataSize);
			if (SUCCEEDED(hrLocal))
			{
				m_buffer[i].AudioBytes = dataSize;
				m_buffer[i].pAudioData = m_DataBuffer[i];
				m_buffer[i].Flags = XAUDIO2_END_OF_STREAM;
				m_buffer[i].LoopCount = m_param[i].bLoop ? XAUDIO2_LOOP_INFINITE : 0;
				loaded = true;
			}
			else
			{
				char buf[256];
				sprintf_s(buf, "[Sound] mp3 load failed idx=%d hr=0x%08X (%s)\n", i, hrLocal, m_param[i].filename);
				OutputDebugStringA(buf);
			}
		}
		else
		{
			HANDLE hFileLocal = CreateFileA(m_param[i].filename, GENERIC_READ, FILE_SHARE_READ, NULL,
				OPEN_EXISTING, 0, NULL);
			if (hFileLocal == INVALID_HANDLE_VALUE)
			{
				char buf[256];
				sprintf_s(buf, "[Sound] wav open failed idx=%d (%s)\n", i, m_param[i].filename);
				OutputDebugStringA(buf);
			}
			else
			{
				// ここは元のFindChunk等の処理をそのまま実行
				DWORD dwChunkSize2, dwChunkPosition2, filetype2;
				SetFilePointer(hFileLocal, 0, NULL, FILE_BEGIN);
				FindChunk(hFileLocal, fourccRIFF, dwChunkSize2, dwChunkPosition2);
				ReadChunkData(hFileLocal, &filetype2, sizeof(DWORD), dwChunkPosition2);

				if (filetype2 == fourccWAVE)
				{
					FindChunk(hFileLocal, fourccFMT, dwChunkSize2, dwChunkPosition2);
					ReadChunkData(hFileLocal, &m_wfx[i], dwChunkSize2, dwChunkPosition2);

					FindChunk(hFileLocal, fourccDATA, dwChunkSize2, dwChunkPosition2);
					m_DataBuffer[i] = new BYTE[dwChunkSize2];
					ReadChunkData(hFileLocal, m_DataBuffer[i], dwChunkSize2, dwChunkPosition2);

					m_buffer[i].AudioBytes = dwChunkSize2;
					m_buffer[i].pAudioData = m_DataBuffer[i];
					m_buffer[i].Flags = XAUDIO2_END_OF_STREAM;
					m_buffer[i].LoopCount = m_param[i].bLoop ? XAUDIO2_LOOP_INFINITE : 0;
					loaded = true;
				}
				CloseHandle(hFileLocal);
			}
		}

		if (loaded)
		{
			m_pXAudio2->CreateSourceVoice(&m_pSourceVoice[i], &(m_wfx[i].Format));
		}
		else
		{
			m_pSourceVoice[i] = nullptr; // 未ロード分は明示的にnullへ
		}
	}

	m_Initialized = true;
	return hr;
}

//=============================================================================
// 開放処理
//=============================================================================
void Sound::Uninit(void)
{
	for (int i = 0; i < SOUND_LABEL_MAX; i++)
	{
		if (m_pSourceVoice[i])
		{
			m_pSourceVoice[i]->Stop(0);
			m_pSourceVoice[i]->FlushSourceBuffers();
			m_pSourceVoice[i]->DestroyVoice();			// オーディオグラフからソースボイスを削除
			m_pSourceVoice[i] = nullptr;
			delete[]  m_DataBuffer[i];
			m_DataBuffer[i] = nullptr;
		}
	}

	if (m_pDynamicVoice)
	{
		m_pDynamicVoice->Stop(0);
		m_pDynamicVoice->FlushSourceBuffers();
		m_pDynamicVoice->DestroyVoice();
		m_pDynamicVoice = nullptr;
	}
	if (m_dynamicData)
	{
		delete[] m_dynamicData;
		m_dynamicData = nullptr;
	}

	if (m_pMasteringVoice)
	{
		m_pMasteringVoice->DestroyVoice();
		m_pMasteringVoice = nullptr;
	}

	if (m_pXAudio2)
	{
		m_pXAudio2->Release();
		m_pXAudio2 = nullptr;
	}

	// MF と COM の終了は、Init が成功したときに1回だけ
	if (m_Initialized)
	{
		MFShutdown();
		CoUninitialize();
		m_Initialized = false;
	}

	MFShutdown();

	// COMの破棄
	CoUninitialize();
}

//=============================================================================
// 再生
//=============================================================================
void Sound::Play(SOUND_LABEL label)
{
	IXAudio2SourceVoice*& pSV = m_pSourceVoice[(int)label];

	if (!pSV) return;

	XAUDIO2_BUFFER buffer = m_buffer[(int)label];
	buffer.PlayBegin = m_playPos[(int)label]; // 一時停止位置から再生
	buffer.PlayLength = 0;
	buffer.LoopCount = m_param[(int)label].bLoop ? XAUDIO2_LOOP_INFINITE : 0;

	pSV->Stop(0);
	pSV->FlushSourceBuffers();
	pSV->SubmitSourceBuffer(&buffer);
	pSV->Start(0);

}

//=============================================================================
// 停止
//=============================================================================
void Sound::Stop(SOUND_LABEL label)
{
	IXAudio2SourceVoice* pSV = m_pSourceVoice[(int)label];
	if (!pSV) return;

	pSV->Stop(0);
	pSV->FlushSourceBuffers();
	m_playPos[(int)label] = 0;
}

//=============================================================================
// 一時停止
//=============================================================================
void Sound::Pause(SOUND_LABEL label)
{
	IXAudio2SourceVoice* pSV = m_pSourceVoice[(int)label];
	if (!pSV) return;

	pSV->Stop(0);

	XAUDIO2_VOICE_STATE state;
	pSV->GetState(&state);
	m_playPos[(int)label] = state.SamplesPlayed; // サンプル単位で位置保存
}

//=============================================================================
// 一時停止
//=============================================================================
void Sound::Resume(SOUND_LABEL label)
{
	Play(label);
}

//=============================================================================
// 再生位置変更
//=============================================================================
void Sound::SetPlayPositionSeconds(SOUND_LABEL label, float seconds)
{
	if (seconds < 0.0f) seconds = 0.0f;

	const int index = (int)label;
	const UINT32 sampleRate = m_wfx[index].Format.nSamplesPerSec;
	const UINT32 blockAlign = m_wfx[index].Format.nBlockAlign;
	const UINT32 totalSamples = blockAlign > 0 ? m_buffer[index].AudioBytes / blockAlign : 0;

	UINT32 sample = (UINT32)(seconds * sampleRate);
	if (totalSamples > 0 && sample >= totalSamples)
	{
		sample = totalSamples - 1;
	}

	m_playPos[index] = sample;
}

void Sound::SeekSeconds(SOUND_LABEL label, float seconds)
{
	SetPlayPositionSeconds(label, seconds);
	Play(label);
}
void Sound::SetVolume(SOUND_LABEL label, float volume)
{
	if (m_pSourceVoice[(int)label] == nullptr) return;

	m_pSourceVoice[(int)label]->SetVolume(volume);
}

HRESULT Sound::LoadDynamic(const std::string& path, bool loop)
{
	HRESULT hr = S_OK;

	if (m_pDynamicVoice)
	{
		m_pDynamicVoice->Stop(0);
		m_pDynamicVoice->FlushSourceBuffers();
		m_pDynamicVoice->DestroyVoice();
		m_pDynamicVoice = nullptr;
	}
	if (m_dynamicData)
	{
		delete[] m_dynamicData;
		m_dynamicData = nullptr;
	}
	m_dynamicPlayPos = 0;
	m_dynamicPlaying = false;
	memset(&m_dynamicWfx, 0, sizeof(m_dynamicWfx));
	memset(&m_dynamicBuffer, 0, sizeof(m_dynamicBuffer));

	std::string ext = path.substr(path.find_last_of(".") + 1);
	for (auto& c : ext) c = (char)tolower((unsigned char)c);

	if (ext == "mp3")
	{
		DWORD dataSize = 0;
		hr = LoadMp3AsPCM(path.c_str(), m_dynamicWfx, m_dynamicData, dataSize);
		if (FAILED(hr))
		{
			OutputDebugStringA(("[Sound] LoadDynamic(mp3) failed: " + path + "\n").c_str());
			return hr;
		}
		m_dynamicBuffer.AudioBytes = dataSize;
		m_dynamicBuffer.pAudioData = m_dynamicData;
		m_dynamicBuffer.Flags = XAUDIO2_END_OF_STREAM;
		m_dynamicBuffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;
	}
	else
	{
		HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, 0, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			OutputDebugStringA(("[Sound] LoadDynamic(wav) file not found: " + path + "\n").c_str());
			return HRESULT_FROM_WIN32(GetLastError());
		}
		if (SetFilePointer(hFile, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
		{
			CloseHandle(hFile);
			return HRESULT_FROM_WIN32(GetLastError());
		}

		DWORD dwChunkSize = 0, dwChunkPosition = 0, filetype = 0;

		FindChunk(hFile, fourccRIFF, dwChunkSize, dwChunkPosition);
		ReadChunkData(hFile, &filetype, sizeof(DWORD), dwChunkPosition);
		if (filetype != fourccWAVE)
		{
			CloseHandle(hFile);
			OutputDebugStringA(("[Sound] LoadDynamic: not a WAVE file: " + path + "\n").c_str());
			return S_FALSE;
		}

		FindChunk(hFile, fourccFMT, dwChunkSize, dwChunkPosition);
		ReadChunkData(hFile, &m_dynamicWfx, dwChunkSize, dwChunkPosition);

		FindChunk(hFile, fourccDATA, dwChunkSize, dwChunkPosition);
		m_dynamicData = new BYTE[dwChunkSize];
		ReadChunkData(hFile, m_dynamicData, dwChunkSize, dwChunkPosition);

		CloseHandle(hFile);

		m_dynamicBuffer.AudioBytes = dwChunkSize;
		m_dynamicBuffer.pAudioData = m_dynamicData;
		m_dynamicBuffer.Flags = XAUDIO2_END_OF_STREAM;
		m_dynamicBuffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;
	}

	hr = m_pXAudio2->CreateSourceVoice(&m_pDynamicVoice, &(m_dynamicWfx.Format));
	if (FAILED(hr))
	{
		OutputDebugStringA(("[Sound] LoadDynamic: CreateSourceVoice failed: " + path + "\n").c_str());
		delete[] m_dynamicData;
		m_dynamicData = nullptr;
		return hr;
	}

	m_dynamicLoop = loop;
	m_dynamicLoadedPath = path;
	OutputDebugStringA(("[Sound] LoadDynamic OK: " + path + "\n").c_str());
	return S_OK;
}

void Sound::PlayDynamic()
{
	if (!m_pDynamicVoice) return;

	XAUDIO2_BUFFER buffer = m_dynamicBuffer;
	buffer.PlayBegin = m_dynamicPlayPos;
	buffer.PlayLength = 0;
	buffer.LoopCount = m_dynamicLoop ? XAUDIO2_LOOP_INFINITE : 0;

	m_pDynamicVoice->Stop(0);
	m_pDynamicVoice->FlushSourceBuffers();
	m_pDynamicVoice->SubmitSourceBuffer(&buffer);
	m_pDynamicVoice->Start(0);

	// 再生位置の追跡を開始
	const UINT32 sampleRate = m_dynamicWfx.Format.nSamplesPerSec;
	m_dynamicStartSec = (sampleRate > 0) ? (float)m_dynamicPlayPos / (float)sampleRate : 0.0f;
	m_dynamicSectionBegin = 0.0f;
	m_dynamicSectionLength = 0.0f;
	m_dynamicStartClock = std::chrono::steady_clock::now();
	m_dynamicPlaying = true;
}

void Sound::StopDynamic()
{
	if (!m_pDynamicVoice) return;
	m_pDynamicVoice->Stop(0);
	m_pDynamicVoice->FlushSourceBuffers();
	m_dynamicPlayPos = 0;
	m_dynamicPlaying = false;
}

void Sound::PauseDynamic()
{
	if (!m_pDynamicVoice) return;
	m_pDynamicVoice->Stop(0);
	m_dynamicPlaying = false;

	XAUDIO2_VOICE_STATE state;
	m_pDynamicVoice->GetState(&state);
	m_dynamicPlayPos = (UINT32)state.SamplesPlayed;
}

void Sound::ResumeDynamic()
{
	PlayDynamic();
}

void Sound::SetPlayPositionSecondsDynamic(float seconds)
{
	if (seconds < 0.0f) seconds = 0.0f;

	const UINT32 sampleRate = m_dynamicWfx.Format.nSamplesPerSec;
	const UINT32 blockAlign = m_dynamicWfx.Format.nBlockAlign;
	const UINT32 totalSamples = blockAlign > 0 ? m_dynamicBuffer.AudioBytes / blockAlign : 0;

	UINT32 sample = (UINT32)(seconds * sampleRate);
	if (totalSamples > 0 && sample >= totalSamples)
		sample = totalSamples - 1;

	m_dynamicPlayPos = sample;
}

void Sound::SeekSecondsDynamic(float seconds)
{
	SetPlayPositionSecondsDynamic(seconds);
	PlayDynamic();
}

void Sound::SetVolumeDynamic(float volume)
{
	if (!m_pDynamicVoice) return;
	m_pDynamicVoice->SetVolume(volume);
}

float Sound::GetDynamicDurationSeconds() const
{
	const UINT32 sampleRate = m_dynamicWfx.Format.nSamplesPerSec;
	const UINT32 blockAlign = m_dynamicWfx.Format.nBlockAlign;
	if (sampleRate == 0 || blockAlign == 0) return 0.0f;

	const UINT32 totalSamples = m_dynamicBuffer.AudioBytes / blockAlign;
	return (float)totalSamples / (float)sampleRate;
}

void Sound::PlayDynamicSection(float beginSec, float lenghtSec)
{
	if (!m_pDynamicVoice) return;

	const UINT32 sampleRate = m_dynamicWfx.Format.nSamplesPerSec;
	const UINT32 blockAlign = m_dynamicWfx.Format.nBlockAlign;
	if (sampleRate == 0 || blockAlign == 0) return;
	const UINT32 totalSamples = m_dynamicBuffer.AudioBytes / blockAlign;
	if (totalSamples == 0) return;

	if (beginSec < 0.0f) beginSec = 0.0f;
	if (lenghtSec < 0.0f) lenghtSec = 0.0f;

	UINT32 begin = (UINT32)(beginSec * sampleRate);
	if (begin >= totalSamples) begin = 0;
	UINT32 length = (UINT32)(lenghtSec * sampleRate);
	if (length == 0 || begin + length > totalSamples) length = totalSamples - begin;

	//再生区間とループ区間を同じにしてループ
	XAUDIO2_BUFFER buffer = m_dynamicBuffer;
	buffer.PlayBegin = begin;
	buffer.PlayLength = length;
	buffer.LoopBegin = begin;
	buffer.LoopLength = length;
	buffer.LoopCount = XAUDIO2_LOOP_INFINITE;

	m_pDynamicVoice->Stop(0);
	m_pDynamicVoice->FlushSourceBuffers();
	m_pDynamicVoice->SubmitSourceBuffer(&buffer);
	m_pDynamicVoice->Start(0);

	m_dynamicSectionBegin = (float)begin / (float)sampleRate;
	m_dynamicSectionLength = (float)length / (float)sampleRate;
	m_dynamicStartSec = m_dynamicSectionBegin;
	m_dynamicStartClock = std::chrono::steady_clock::now();
	m_dynamicPlaying = true;
}

float Sound::GetDynamicPlaySeconds() const
{
	if (!m_dynamicPlaying) return m_dynamicStartSec;

	float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_dynamicStartClock).count();

	// 区間ループ中
	if (m_dynamicSectionLength > 0.0f)
		return m_dynamicSectionBegin + fmodf(elapsed, m_dynamicSectionLength);

	// 通常再生（曲全体）
	float t = m_dynamicStartSec + elapsed;
	float duration = GetDynamicDurationSeconds();
	if (duration > 0.0f)
	{
		if (m_dynamicLoop) t = fmodf(t, duration);
		else if (t > duration) t = duration;
	}
	return t;
}

//=============================================================================
// ユーティリティ関数群
//=============================================================================
HRESULT Sound::FindChunk(HANDLE hFile, DWORD fourcc, DWORD& dwChunkSize, DWORD& dwChunkDataPosition)
{
	HRESULT hr = S_OK;
	if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, 0, NULL, FILE_BEGIN))
		return HRESULT_FROM_WIN32(GetLastError());
	DWORD dwChunkType;
	DWORD dwChunkDataSize;
	DWORD dwRIFFDataSize = 0;
	DWORD dwFileType;
	DWORD bytesRead = 0;
	DWORD dwOffset = 0;
	while (hr == S_OK)
	{
		DWORD dwRead;
		if (0 == ReadFile(hFile, &dwChunkType, sizeof(DWORD), &dwRead, NULL))
			hr = HRESULT_FROM_WIN32(GetLastError());
		if (0 == ReadFile(hFile, &dwChunkDataSize, sizeof(DWORD), &dwRead, NULL))
			hr = HRESULT_FROM_WIN32(GetLastError());
		switch (dwChunkType)
		{
		case fourccRIFF:
			dwRIFFDataSize = dwChunkDataSize;
			dwChunkDataSize = 4;
			if (0 == ReadFile(hFile, &dwFileType, sizeof(DWORD), &dwRead, NULL))
				hr = HRESULT_FROM_WIN32(GetLastError());
			break;
		default:
			if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, dwChunkDataSize, NULL, FILE_CURRENT))
				return HRESULT_FROM_WIN32(GetLastError());
		}
		dwOffset += sizeof(DWORD) * 2;
		if (dwChunkType == fourcc)
		{
			dwChunkSize = dwChunkDataSize;
			dwChunkDataPosition = dwOffset;
			return S_OK;
		}
		dwOffset += dwChunkDataSize;
		if (bytesRead >= dwRIFFDataSize) return S_FALSE;
	}
	return S_OK;
}

HRESULT Sound::ReadChunkData(HANDLE hFile, void* buffer, DWORD buffersize, DWORD bufferoffset)
{
	HRESULT hr = S_OK;
	if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, bufferoffset, NULL, FILE_BEGIN))
		return HRESULT_FROM_WIN32(GetLastError());
	DWORD dwRead;
	if (0 == ReadFile(hFile, buffer, buffersize, &dwRead, NULL))
		hr = HRESULT_FROM_WIN32(GetLastError());
	return hr;
}

HRESULT Sound::LoadMp3AsPCM(LPCSTR filename, WAVEFORMATEXTENSIBLE& outWfx, BYTE*& outData, DWORD& outSize)
{
	HRESULT hr = S_OK;

	// ファイルパスをワイド文字列に変換
	wchar_t wpath[MAX_PATH];
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, wpath, MAX_PATH);

	IMFSourceReader* pReader = nullptr;
	hr = MFCreateSourceReaderFromURL(wpath, nullptr, &pReader);
	if (FAILED(hr)) return hr;

	// 出力フォーマットをPCM(非圧縮)に指定する
	IMFMediaType* pPCMType = nullptr;
	MFCreateMediaType(&pPCMType);
	pPCMType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	pPCMType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPCMType);

	// 実際に決定されたフォーマットを取得
	IMFMediaType* pActualType = nullptr;
	pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pActualType);

	WAVEFORMATEX* pWfx = nullptr;
	UINT32 wfxSize = 0;
	MFCreateWaveFormatExFromMFMediaType(pActualType, &pWfx, &wfxSize);
	memcpy(&outWfx, pWfx, std::min(sizeof(WAVEFORMATEXTENSIBLE), (size_t)wfxSize));
	CoTaskMemFree(pWfx);

	// サンプルを1つずつ読み込んでPCMデータを連結していく
	std::vector<BYTE> pcmData;
	while (true)
	{
		DWORD flags = 0;
		IMFSample* pSample = nullptr;
		hr = pReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
			0, nullptr, &flags, nullptr, &pSample);
		if (FAILED(hr)) break;
		if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
		if (!pSample) continue;

		IMFMediaBuffer* pBuffer = nullptr;
		pSample->ConvertToContiguousBuffer(&pBuffer);

		BYTE* pAudioData = nullptr;
		DWORD dataLen = 0;
		pBuffer->Lock(&pAudioData, nullptr, &dataLen);
		pcmData.insert(pcmData.end(), pAudioData, pAudioData + dataLen);
		pBuffer->Unlock();

		pBuffer->Release();
		pSample->Release();
	}

	outSize = (DWORD)pcmData.size();
	outData = new BYTE[outSize];
	memcpy(outData, pcmData.data(), outSize);

	pActualType->Release();
	pPCMType->Release();
	pReader->Release();

	return S_OK;
}
