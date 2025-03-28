from glob import glob
import os
from time import sleep

path = "/home/kutluhan/Desktop/audio_samples"
audio_files = glob(path + "/*.3gp")

for audio in audio_files:
    new_path = audio.replace("audio_samples/", "audio_samples/wav/")
    new_path = new_path.replace(".3gp", ".wav")
    os.system('ffmpeg -i ' + audio + ' ' + new_path)