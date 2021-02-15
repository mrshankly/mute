mute
====

🤫

Simple tool that allows you to toggle mute your audio and microphone with ease. It uses pulseaudio's default sink and source.

## Building

Pulseaudio must be installed and running.

Run `make` and move the resulting binary, `mute`, into a directory that belongs to your PATH.

## Usage

```
usage: mute [-m]
```

If the microphone is unmuted, `mute -m` mutes the microphone and leaves the audio unchanged. If the microphone is muted, `mute -m` unmutes the microphone and audio.

If the audio is unmuted, `mute` mutes the audio and microphone. If the audio is muted, `mute` unmutes the audio and microphone, but only if the microphone wasn't explicitly muted by a `mute -m`.
