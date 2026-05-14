# YellowedPlasticSQL - PostgreSQL client for C64

YellowedPlasticSQL, `ypsql` for short, is a PostgreSQL client running natively
on the Commodore 64 using RR-net and IP65 for network access.  It implements a
subset of the PostgreSQL wire protocol, with more support planned for future
updates.

The app is still under heavy development, it was written over the course of a
weekend for the PostgreSQL project 30 year anniversary celebration at
[PGConf.dev 2026](https://2026.pgconf.dev).  It can log in using password, or
trust, authentication and can issue queries.  Errorhandling is still very
limited but will be improved.

## Querying

Queries are not ended with semicolon as that is hard to type on a C64 keyboard,
the code will inject the semicolon as needed.  The query will is sent with
ENTER.

## Metacommands

Unlike `psql` ypsql use `:` instead of backslash for the metacommands since
it's a lot more convenient on a C64 keyboard. The available metacommands are:

| Command | Description                                   |
|---------|-----------------------------------------------|
| :clear  | Clear the screen                              |
| :help   | Print helpscreen                              |
| :host   | Print address and port of server connected to |
| :quit   | Logout and quit                               |
| :watch  | Run last query every 1s                       |

## TODO

- [ ] Working ASCII ot PETSCII conversion
- [ ] Pretty table printing using PETSCII characters
- [ ] Improved errorhandling to handle misformed queries
- [ ] Code optimization
- [ ] MD5 Password auth using [md5_6502](https://github.com/laubzega/md5_6502)
- [ ] SCRAM authentication using [sha256_6502](https://github.com/laubzega/sha256_6502/) and [8-bit sha1](https://github.com/orix-software/sha1/blob/master/src/main.c) for HMAC.
- [ ] Split up the code in libpq and app code for easier porting to Apple II

## Dependencies

* [cc65](https://cc65.github.io/) compiler suite
* [ip65](https://github.com/cc65/ip65) for networking

## License

YellowedPlasticSQL use the [PostgreSQL license](https://opensource.org/license/postgresql)
which is an OSI approved license.  See LICENSE for full details.
