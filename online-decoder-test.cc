#include "onlinedecoder/audio-buffer-source.h"
#include "onlinedecoder/online-decoder.h"
#include "feat/wave-reader.h"
#include <stdio.h>
#include <thread>
#include "onlinedecoder/speech-recognition-engine.h"

using namespace kaldi;
using namespace fst;

typedef kaldi::int32 int32;
typedef kaldi::int64 int64;

void OnSentence(const char* pszResults)
{
	KALDI_LOG << "result : " << pszResults;
}

void ReadData(const std::string& wav_rspecifier, int32 chunk_length_secs, int engineID)
{
	SequentialTableReader<WaveHolder> wav_reader(wav_rspecifier);
	for (; !wav_reader.Done(); wav_reader.Next()) {
		std::string utt = wav_reader.Key();
		KALDI_LOG << "utterance id : " << utt;
		const WaveData &wave_data = wav_reader.Value();
		KALDI_LOG << "1";
		// get the data for channel zero
		SubVector<BaseFloat> data(wave_data.Data(), 0);
		BaseFloat samp_freq = wave_data.SampFreq();
		int32 chunk_length;
		if (chunk_length_secs > 0) {
			chunk_length = int32(samp_freq * chunk_length_secs);
			if (chunk_length == 0) chunk_length = 1;
		}
		else {
			chunk_length = std::numeric_limits<int32>::max();
		}
    KALDI_LOG << "2";
		int32 samp_offset = 0;
		OnlineTimer decoding_timer(utt);
		while (samp_offset < data.Dim())
		{
			int32 samp_remaining = data.Dim() - samp_offset;
			int32 num_samp = chunk_length < samp_remaining ? chunk_length : samp_remaining;
			short *pBuffer = new short[num_samp];
			KALDI_LOG << "3";
			for (int32 i = 0; i < num_samp; ++i)
				pBuffer[i] = data(samp_offset + i);
		  KALDI_LOG << "4";
			AddBuffer(engineID, utt.c_str(), pBuffer, num_samp);
			KALDI_LOG << "5";
			samp_offset += num_samp;
			decoding_timer.WaitUntil(samp_offset / samp_freq);
		}
	}
}

int main(int argc, char *argv[]) {
  try {
    const char *usage = "online decoder test.\n"
                        "online-decoder-test mandarin.conf "
                        "\"ark,s,cs:../featbin/wav-copy scp,p:data/wav.scp ark:- |\"\n";
    ParseOptions po(usage);
    
    BaseFloat chunk_length_secs = 0.05;
    po.Register("chunk-length", &chunk_length_secs,
                "Length of chunk size in seconds, that we process.  Set to <= 0 "
                "to use all input in one chunk.");
                
    po.Read(argc, argv);            
    if (po.NumArgs() != 2) {
      po.PrintUsage();
      return 1; 
    }
    
    std::string conf_rxfilename = po.GetArg(1),
                wav_rxfilename = po.GetArg(2);
                
	// create engine
	int engineID = CreateRecognizer(conf_rxfilename.c_str());
		
	// add callbackfor FINAL_RESULT_SIGNAL
	AddCallback(engineID, FINAL_RESULT_SIGNAL, OnSentence);

	// firstly, start the decoder
	StartRecognizer(engineID);

	// secondly, start the data thread
	std::thread data_thread(ReadData, wav_rxfilename, 5, engineID);

	// wait for user command
	while (true)
	{
		std::cout << "(1) supspend recognizer";
		std::cout << "(2) resume recognizer";
		std::cout << "(3) stop recognizer";
		std::cout << ":";
		char c[1] = "";
		std::cin >> c;
		switch (c[0])
		{
		case '1':
			SuspendRecognizer(engineID);
			break;
		case '2':
			ResumeRecognizer(engineID);
			break;
		case '3':
			StopRecognizer(engineID);
			break;
		}
		if (c[0] == '3')
			break;
	}

	// wait for the recognizing thread to stop
	WaitForRecogStop(engineID);

	// destroy the recognizer object
	FreeRecognizer(engineID);

  } catch(const std::exception& e) {
    std::cerr << e.what();
    return -1;
  }
}
