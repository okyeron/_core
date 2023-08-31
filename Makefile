dobuild: pico-extras lib/biquad.h build
	cd build && PICO_EXTRAS_PATH=../pico-extras make -j4

lib/biquad.h:
	cd lib && python3 biquad.py > biquad.h

libs:
	cd lib && python3 transfer_saturate.py > transfer_saturate.h
	cd lib && python3 transfer_distortion.py > transfer_distortion.h
	cd lib && python3 selectx.py > selectx.h
	cd lib && python3 biquad.py > biquad.h
	cd lib && python3 crossfade.py > crossfade.h


pico-extras:
	git clone https://github.com/raspberrypi/pico-extras.git
	cd pico-extras && git submodule update -i 

upload: dobuild
	./dev/upload.sh 

build:
	rm -rf build
	mkdir build
	cd build && PICO_EXTRAS_PATH=../pico-extras cmake ..
	cd build && PICO_EXTRAS_PATH=../pico-extras make -j32
	echo "build success"

audio:
	sox lib/audio/amen_5c2d11c8_beats16_bpm170.flac -c 1 --bits 16 --encoding signed-integer --endian little amen_bpm170_beats16_mono.wav
	sox lib/audio/amen_0efedaab_beats8_bpm165.flac -c 1 --bits 16 --encoding signed-integer --endian little amen_bpm165_beats8_mono.wav

audio2:
	sox lib/audio/amen_5c2d11c8_beats16_bpm170.flac -c 2 --bits 16 --encoding signed-integer --endian little amen_bpm170_beats16_stereo.wav
	sox lib/audio/amen_0efedaab_beats8_bpm165.flac -c 2 --bits 16 --encoding signed-integer --endian little amen_bpm165_beats8_stereo.wav

bass:
	sox lib/audio/bass_e.wav /tmp/1.wav fade 0.001 -0 0.001 norm gain -6
	gcc -o /tmp/convert lib/convert.c
	rm -rf lib/bass_raw.h
	touch lib/bass_raw.h
	sox /tmp/1.wav -c 1 --bits 16 --encoding signed-integer --endian little /tmp/0.wav
	sox /tmp/0.wav -t raw /tmp/0.raw
	/tmp/convert /tmp/0.raw 0 >> lib/bass_raw.h
	sox /tmp/0.wav /tmp/1.wav speed 2.0
	sox /tmp/1.wav -t raw /tmp/1.raw
	/tmp/convert /tmp/1.raw 1 >> lib/bass_raw.h

clean:
	rm -rf build
	rm -rf *.wav
	rm -rf lib/biquad.h

