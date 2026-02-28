def can_build(env, platform):
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "AudioStreamMIDI",
        "AudioStreamPlaybackMIDISF2",
        "AudioStreamSoundfontPlayer",
        "AudioStreamPlaybackSoundfont",
        "MIDI",
        "SoundFont2",
        "VirtualKeyboard",
    ]


def get_doc_path():
    return "doc_classes"

def get_icons_path():
    return "project/addons/godot-midi/icons"