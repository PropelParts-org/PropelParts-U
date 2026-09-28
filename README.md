# PropelParts U

## Overview
A [RedCore](https://github.com/Zenith-Team/RedCore) module consisting of [PropelParts](https://github.com/PropelParts-org/PropelParts) actors ported to NSMBU.
Currently unfinished and does not support the current version of RedCore. Do not expect this to be usable at the current moment.

## Setup Guide
### Compiling
Clone [RedCore](https://github.com/Zenith-Team/RedCore), as this module requires RedCore 2.0.0 (currently in development, so you must provide your own copy)
Install [Tachyon](https://github.com/Zenith-Team/Tachyon) (requires [Node.js](https://nodejs.org/) v24+)
```yml
npm i -g --allow-remote=root https://github.com/Zenith-Team/Tachyon/releases/latest/download/tachyon.tgz
```
Build and run the project for your region (example with `US`)
```rb
tachyon pm link [PATH_TO_LOCAL_REDCORE_COPY]
tachyon compile US
tachyon launch US
```