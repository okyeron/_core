MIDI Implementation Chart

| Guide  |       |
| ---    | ---   |
| O:     |  Yes  |
| x:     |   No  |

| Function              |      Transmitted      |       Recognized        |     Remarks                    |
| ----------------------|:---------------------:|:-----------------------:|--------------------------------|
| MIDI channels         |          1–16         |           1–16          |                                |
| Note numbers          |           x           |            x            |                                |
| Program change        |           x           |            x            |                                |
| Bank select           |           x           |            x            |  See CC-9 for Sample Select    |
| Velocity Note-On      |           x           |            x            |                                |
| Velocity Note-Off     |           x           |            x            |                                |
| Channel Aftertouch    |           x           |            x            |                                |
| Poly (Key) Aftertouch |           x           |            x            |                                |
| Pitch Bend            |           x           |            x            |                                |
| -                     |                       |                         |                                |
| System Exclusive      |           O           |            x            |                                |
| ......................|.......................|.........................|................................|


| System Real/Common    |                       |                         |                                |
| ----------------------|:---------------------:|:-----------------------:|--------------------------------|
| MIDI Clock            |           O           |            O            |                                |
| Start                 |           O           |            O            |                                |
| Continue              |           O           |            O            |                                |
| Stop                  |           O           |            O            |                                |
| Song Position Pointer |           x           |            x            |                                |
| Song Select           |           x           |            x            |                                |
| -                     |                       |                         |                                |
| All Notes Off         |           x           |            x            |                                |
| System Reset          |           x           |            x            |                                |
| ......................|.......................|.........................|................................|

| Control               |      Transmitted      |       Recognized        |     Remarks |
| ----------------------|:---------------------:|:-----------------------:|--------------------------------|
| 0                     |           O           |            x            |    Knob X                      |
| 1                     |           O           |            x            |    Knob Y                      |
| 2                     |           O           |            x            |    Knob Z                      |
| 3                     |           O           |            x            |    Tempo (A+X)                 |
| 4                     |           O           |            x            |    Pitch (A+Y)                 |
| 5                     |           O           |            x            |    Volume (A+Z)                |
| 6                     |           O           |            x            |    Rand Sequence (B+X)         |
| 7                     |           O           |            x            |    Filter (B+Y)                |
| 8                     |           O           |            x            |    Bass Volume (B+Z)           |
| 9                     |           O           |            x            |    Sample Select (C+X)         |
| 10                    |           O           |            x            |    Rand Tunnel (C+Y)           |
| 11                    |           O           |            x            |    Quant (C+Z)                 |
| 12                    |           O           |            x            |    Rand Jump2 (D+X)            |
| 13                    |           O           |            x            |    Rand FX (D+Y) - is this Grimoire Prob?|
| 14                    |           O           |            x            |    Rand FX bank (D+Z) - is this Grimoire?|
| ......................|.......................|.........................|................................|
