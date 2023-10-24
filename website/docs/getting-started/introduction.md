# Introduction

## Foreword

Welcome to the DOSBox Staging getting started guide!

This guide will gently introduce you to the wonderful world of DOSBox by
setting up a few example games from scratch. It's written in the spirit of
"teaching a man how to fish"---the games are only vehicles to teach you the
basics that you can apply to any DOS game you wish to play later.
Consequently, the choice of games doesn't matter that much (although we tried
to pick from the all-time classics).

To get the most out of this guide, don't just *read* the instructions but
*perform* all the steps yourself! Later chapters build on concepts introduced
in previous ones, so *do not skip a chapter* just because you're not
interested in a particular game! You don't have to play it if you don't want
to; going through the setup procedure and learning how to troubleshoot various
issues is what this is all about. All necessary files will be provided and no
familiarity with IBM PCs and the MS-DOS environment is required---everything
you need to know will be explained as we go. The only assumption is that you
can perform basic everyday computer tasks, such as copying files, unpacking
ZIP archives, and editing text files. 

The guide has been written so that everyone can follow it with ease,
regardless of their operating system of choice (e.g., Windows users probably
know what the "C drive" is, and most Linux people are comfortable with using
the command line, but these things need to be explained to the Mac folks).

We wish you a pleasant journey and we hope DOSBos Staging will bring you as
much joy as we're having developing it!


## Installing DOSBox Staging

If you already have other versions of DOSBox on your computer, installing
DOSBox Staging won't interfere with them at all. Experienced users can
certainly use multiple DOSBox variants on the same machine without problems,
but if you're a beginner, we recommend starting with a clean slate to avoid
confusing yourself. Make sure you've removed all other DOSBox versions from
your machine first, then proceed with the DOSBox Staging installation steps.

<h3>Windows</h3>

Download the latest installer from our [Windows
downloads](../downloads/windows.md) page, then proceed with the installation.
Just accept the default options, don't change anything.

Make sure to read the section about dealing with [Microsoft Defender SmartScreen](../downloads/windows.md#microsoft-defender-smartscreen).

<h3>macOS</h3>

Download the latest universal binary from our [macOS
downloads](../downloads/macos.md) page, then simply drag the DOSBox Staging
icon into your Applications folder. Both Intel and M1 Macs are supported.

Don't delete the `.dmg` installer disk image just yet---we'll need it later.

Make sure to read the section about dealing with [Apple Gatekeeper](../downloads/macos.md#apple-gatekeeper).


<h3>Linux</h3>

Please check out our [Linux downloads](../downloads/linux.md) page and use
the option that best suits your situation and needs.


## Other stuff we'll need

As you follow along, you'll need to create and edit DOSBox configuration files
which are plain text files. While Notepad on Windows or TextEdit on macOS
could do the job, it's preferable to use a better text editor more suited to
the task.


<h3>Windows</h3>

Windows users are recommended to install the free and open-source
[Notepad++](https://notepad-plus-plus.org/) editor.

Accept the default installer options; this will give you a handy *Open with
Notepad++* right-click context menu entry in Windows Explorer.

<figure markdown>
  ![Editing a DOSBox configuration file in Notepad++](https://archive.org/download/dosbox-staging-getting-started-guide-assets/notepad++.png){ loading=lazy }
</figure>


<h3>macOS</h3>

[TextMate](https://macromates.com/) is a free and open-source text editor
for macOS which is perfect for the job.

!!! tip

    When editing DOSBox configuration files in TextMate, it's best to set
    syntax highlighting to the *Properties* file format as shown in the below
    screenshot (it's the combo-box to the left of the *Tab Size* combo-box in
    the footer).

<figure markdown>
  ![Editing a DOSBox configuration file in TextMate](https://archive.org/download/dosbox-staging-getting-started-guide-assets/textmate.png){ loading=lazy }
</figure>


<h3>Linux</h3>

We're pretty sure Linux users don't need any help and have their favourite
text editors already. And, of course, everyone knows **vim** is the best!
:sunglasses:

