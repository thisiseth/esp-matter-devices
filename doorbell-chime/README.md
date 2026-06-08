#### Home Assistant

as of today Home Assistant does not support Matter Chime on the frontend side, however the Matter Server addon does.
the workaround i use is to directly poke the Matter Server websocket to call the chime play function using a shell command

1. put `chime_play.py` to `/config/scripts/chime_play.py`
2. add a shell command to execute that script to `configuration.yaml`:
```
    shell_command:
      chime_play: "python3 /config/scripts/chime_play.py '{{node_id}}'"
```
4. either use automation or make a helper button with action `shell_command.chime_play` and pass the matter node_id as data
```
    action: shell_command.chime_play
    data:
      node_id: >-
        {% set identifiers = device_attr('your device id, something like 838cb097b031f45805a48a4f49e5cb3c', 'identifiers') %}
        {% set identifier = identifiers | selectattr('0', 'eq', 'matter') | list | first %}
        {{ identifier[1].split('-')[1] | int(base=16) }}
```

#### audio.wav

chime audio is embedded into the firmware when built and should be located at `/doorbell_chime/main/audio.wav`

everything is hardcoded to 16bit mono 44.1k simple wav format -
to create your own `audio.wav` use `convert_to_wav.py` script

1. ffmpeg required - `pip install imageio-ffmpeg`
2. `python3 convert_to_wav.py input_file_of_any_ffmpeg_supported_format output_file start_from_second duration_in_seconds`

On a 4mb flash i have around 2000k free, thats about 20s of audio - plenty for a doorbell
