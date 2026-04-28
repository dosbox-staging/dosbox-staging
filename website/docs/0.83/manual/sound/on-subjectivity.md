# On subjectivity

While few people would choose PC speaker audio when better options are
available, the "best" choice becomes genuinely subjective as you move up the
[list of preferred sound devices](overview.md#selecting-the-best-audio-option)
— and the differences between them run deeper than you might think.

To save cost and effort, developers usually commissioned musicians to create
the soundtrack for the most advanced audio device their game had support for.
Music for the other "lesser" devices had usually been derived from these
original compositions, often by secondary composers, or in some cases by the
programmers themselves. The worst possible solution was to let an audio
middleware library called Miles Sound System, widely used by DOS developers to
handle audio across multiple devices, do an automatic by-the-numbers
translation from General MIDI to OPL --- a wholly disappointing practice found
in some late DOS-era games. As a result of these different approaches, the
quality of these derived soundtracks vary greatly between games and
development studios.

For example, Sierra On-Line was famous for attracting high-profile musicians
to compose the Roland MT-32 "master" soundtracks for many of their post-1987
adventure games. Their first game with Roland MT-32 support was
[King's Quest IV: The Perils of Rosella](https://www.mobygames.com/game/129/kings-quest-iv-the-perils-of-rosella/)
released in 1988, which was the first PC game to feature a fully-orchestrated
soundtrack written by a professional media composer, William Goldstein.

Similarly, the music of [Police Quest III: The
Kindred](https://www.mobygames.com/game/148/police-quest-3-the-kindred/) from
1991 was composed by Jan Hammer of Miami Vice fame specifically for the Roland
MT-32. Jan Hammer had a particular affinity for the LA synthesis of the Roland
D-50 --- the MT-32's bigger sibling, and a defining sound of 1980s pop music.
Sierra simply couldn't have found a more suitable musician for the job!

Most people could only afford an AdLib or a Sound Blaster back in the day, but
given that now everybody has the option to listen to these soundtracks in
their original form --- true to the composers' intentions. Not doing so feels
almost like a sacrilege.

But the picture isn't always this clear-cut. Some games were composed
primarily for OPL synthesis, with MT-32 or General MIDI support added as an
afterthought. Others have OPL soundtracks with such character and charm that
many players genuinely prefer them over the "objectively better" MIDI
versions. There's an argument that smooth, richly articulated MT-32 or General
MIDI music can feel strangely at odds with the chunky pixel art of a
320&times;200 game --- a kind of uncanny valley where the audio and visuals
seem to belong to different worlds. Sometimes the gritty texture of OPL just
fits better. And a handful of titles simply sound best on the Gravis
UltraSound or with raw FM synthesis.

The takeaway is simple: always try the different audio options your game
supports. Start with the highest-ranked option from the [suggested
preferences](overview.md#selecting-the-best-audio-option), but don't be
afraid to switch if another device sounds better to your ears. The "right"
choice is whichever one brings you the most enjoyment.
