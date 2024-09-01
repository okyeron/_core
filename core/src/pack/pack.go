package pack

import (
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"time"

	"github.com/schollz/_core/core/src/utils"
	"github.com/schollz/_core/core/src/zeptocore"
	log "github.com/schollz/logger"
)

var Storage = "zips"

type Data struct {
	Oversampling              string   `json:"oversampling"`
	StereoMono                string   `json:"stereoMono"`
	Resampling                string   `json:"resampling"`
	SettingsBrightness        int      `json:"settingsBrightness"`
	SettingsClockStop         bool     `json:"settingsClockStop"`
	SettingsClockOutput       bool     `json:"settingsClockOutput"`
	SettingsClockBehaviorSync bool     `json:"settingsClockBehaviorSync"`
	SettingsKnobXSample       bool     `json:"settingsKnobXSample"`
	SettingsGrimoireEffects   [][]bool `json:"settingsGrimoireEffects"`
	Banks                     []struct {
		Files []string `json:"files"`
	} `json:"banks"`
}

func Zip(pathToStorage string, payload []byte, settingsOnly bool) (zipFilename string, err error) {
	zipStorage := path.Join(pathToStorage, "zips")
	os.MkdirAll(zipStorage, 0777)

	// get all the files
	// each file is a folder inside pathToStorage
	var data Data
	err = json.Unmarshal(payload, &data)
	if err != nil {
		log.Error(err)
		return
	}
	log.Tracef("data: %+v", data)
	log.Debugf("oversampling: %s", data.Oversampling)
	log.Debugf("stereoMono: %s", data.StereoMono)
	log.Debugf("resampling: %s", data.Resampling)
	log.Debugf("settingsBrightness: %d", data.SettingsBrightness)
	log.Debugf("settingsClockStop: %v", data.SettingsClockStop)
	log.Debugf("settingsGrimoireEffects: %+v", data.SettingsGrimoireEffects)

	oversampling := 1
	if data.Oversampling == "2x" {
		oversampling = 2
	} else if data.Oversampling == "4x" {
		oversampling = 4
	}

	_, zipFilename = filepath.Split(pathToStorage)

	// create a temporary folder to store the files
	err = os.MkdirAll(path.Join(zipStorage, zipFilename), 0777)
	if err != nil {
		return
	}

	if !settingsOnly {
		// create the bank folders
		for i, bank := range data.Banks {
			log.Tracef("bank %d has %d files", i, len(bank.Files))
			if len(bank.Files) == 0 {
				continue
			}
			// process each file according to parameters
			for _, file := range bank.Files {
				log.Tracef("bank %d: %s", i, file)
				// get the file information
				var f zeptocore.File
				f, err = zeptocore.Get(path.Join(pathToStorage, file, file))
				if err != nil {
					log.Error(err)
					return
				}
				f.SetOversampling(oversampling)
			}
		}
		time.Sleep(200 * time.Millisecond)
	}

	// wait until all the files are processed
	for i := 0; i < 3000; i++ {
		time.Sleep(100 * time.Millisecond)
		if !zeptocore.IsBusy() {
			break
		}
	}
	if zeptocore.IsBusy() {
		err = fmt.Errorf("could not process all files")
		log.Error(err)
		return
	}

	mainFolder := path.Join(zipStorage, zipFilename)
	err = os.MkdirAll(mainFolder, 0777)
	if err != nil {
		log.Error(err)
		return
	}
	settingsFolder := path.Join(mainFolder, "settings")
	err = os.MkdirAll(settingsFolder, 0777)
	if err != nil {
		log.Error(err)
		return
	}
	if data.Resampling == "linear" {
		os.Create(path.Join(settingsFolder, "resampling_quadratic-off"))
	} else {
		os.Create(path.Join(settingsFolder, "resampling_quadratic-on"))
	}
	if data.SettingsClockStop {
		os.Create(path.Join(settingsFolder, "clock_stop_sync-on"))
	} else {
		os.Create(path.Join(settingsFolder, "clock_stop_sync-off"))
	}
	if data.SettingsClockOutput {
		os.Create(path.Join(settingsFolder, "clock_output_trig-on"))
	} else {
		os.Create(path.Join(settingsFolder, "clock_output_trig-off"))
	}

	if data.SettingsClockBehaviorSync {
		os.Create(path.Join(settingsFolder, "clock_behavior_sync_slice-on"))
	} else {
		os.Create(path.Join(settingsFolder, "clock_behavior_sync_slice-off"))
	}

	// knob x
	if data.SettingsKnobXSample {
		os.Create(path.Join(settingsFolder, "knobx_select_sample-on"))
	} else {
		os.Create(path.Join(settingsFolder, "knobx_select_sample-off"))
	}
	// brightness
	os.Create(path.Join(settingsFolder, fmt.Sprintf("brightness-%d", data.SettingsBrightness)))

	// grimoire effects
	for i, effect := range data.SettingsGrimoireEffects {
		os.MkdirAll(path.Join(settingsFolder, "grimoire", fmt.Sprintf("rune%d", i+1)), 0777)
		for j, on := range effect {
			if on {
				os.Create(path.Join(settingsFolder, "grimoire", fmt.Sprintf("rune%d", i+1), fmt.Sprintf("effect%d-on", j+1)))
			} else {
				os.Create(path.Join(settingsFolder, "grimoire", fmt.Sprintf("rune%d", i+1), fmt.Sprintf("effect%d-off", j+1)))
			}
		}
	}

	if !settingsOnly {
		// copy files
		for i, bank := range data.Banks {
			if len(bank.Files) == 0 {
				continue
			}
			log.Tracef("bank %d has %d files", i, len(bank.Files))
			bankFolder := path.Join(zipStorage, zipFilename, fmt.Sprintf("bank%d", i))
			err = os.MkdirAll(bankFolder, 0777)
			if err != nil {
				return
			}
			// go through each file and copy it into the bank
			for filei, file := range bank.Files {
				log.Tracef("bank %d: %s", i, file)
				filenameWithoutExtension := file[:len(file)-len(path.Ext(file))]
				for i := 0; i < 100; i++ {
					oldFname := path.Join(pathToStorage, file, fmt.Sprintf("%s.%d.wav", filenameWithoutExtension, i))
					newFname := path.Join(bankFolder, fmt.Sprintf("%d.%d.wav", filei, i))
					if _, err := os.Stat(oldFname); os.IsNotExist(err) {
						break
					}
					// copy wav file
					err = utils.CopyFile(oldFname, newFname)
					if err != nil {
						log.Error(err)
						return
					}
					// copy info file
					err = utils.CopyFile(oldFname+".info", newFname+".info")
					if err != nil {
						log.Error(err)
						return
					}

					// create tape emulations
					if i == 1 {
						emulations := []string{"TC-260", "808 Comp and Tone", "That Dirty LoFi", "Old Telephone"}
						for i, emulation := range emulations {
							log.Tracef("emulation: %s on file %d", emulation, filei)
							cmd := exec.Command("lv2file", "-i", path.Join(bankFolder, fmt.Sprintf("%d.%d.wav", filei, 0)), "-o", path.Join(bankFolder, fmt.Sprintf("%d.%d.wav", filei, 2+i*2)), "-P", emulation, "9")
							err = cmd.Run()
							if err != nil {
								log.Error(err)
							}
							cmd = exec.Command("lv2file", "-i", path.Join(bankFolder, fmt.Sprintf("%d.%d.wav", filei, 1)), "-o", path.Join(bankFolder, fmt.Sprintf("%d.%d.wav", filei, 3+i*2)), "-P", emulation, "9")
							err = cmd.Run()
							if err != nil {
								log.Error(err)
							}
						}

					}
				}
			}
		}
	}

	// zip the folder
	cwd, _ := os.Getwd()
	os.Chdir(zipStorage)
	err = utils.ZipFolder(zipFilename)
	if err != nil {
		log.Error(err)
	}

	// remove the directory
	err = os.RemoveAll(zipFilename)
	if err != nil {
		log.Error(err)
	}

	os.Chdir(cwd)

	zipFilename = path.Join(zipStorage, zipFilename+".zip")
	return
}
