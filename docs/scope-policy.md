# What this repository will not ship

A boundary, written down so it does not depend on remembering it.

## The rule

**Nothing that lets a camera report an identity or a history that is not its own.**

This is not about secrecy. Protocol details are uninteresting on their own. The
harm is in the capability: a documented opcode is a curiosity; a working writer is
a machine for laundering used bodies.

## Specifically excluded

| capability | why |
|---|---|
| writing the camera's serial number / unit id | makes a body indistinguishable from another; the serial also derives the Wi-Fi password, so it is an identity in the strong sense |
| writing the shutter actuation count | the primary number buyers use to price a body; rewriting it is straightforward fraud |
| writing usage counters, error-log history, service records | same category - they are the object's biography |
| writing a serial into photograph metadata so the file reports a different body | forges the provenance of an image rather than the object, but it is the same lie and it is the one that survives after the camera is sold |
| a generic "write any camera parameter" interface | the capability is the union of everything above, whatever it is pointed at |

The last row is the important one. A tool that can write an arbitrary parameter is
not made safe by declining to name the dangerous ones.

## What is welcome

- reading anything the camera will report about itself
- decoding and re-encoding packages, and understanding the container
- crop geometry, lens correction, colour science, film recipes
- documentation of firmware structure, and of what is *not* known
- tooling whose effect is on the developer's own files, not on a camera's identity

## How this was applied

The repository previously carried a `reported_serial` feature: a selection field,
a CLI flag and a UI input that made the identity demo write a caller-chosen serial
into photograph metadata, while preserving the original alongside it.

Preserving the original did not make it acceptable. The output is a file that
claims to come from a camera it did not come from, and the "original is retained"
property is invisible to anyone reading the photograph.

It was removed in `c6764cb`, together with the dual original/reported identity
model, the A431/private serialization path and their tests.

## Where the research lives

Reverse-engineering work on the camera's control surface, including the PTP
operation catalogue, the session gate and the parameter read/write pair, is kept in
a private tree and is not published here. That is a deliberate choice, not an
oversight, and it is not a temporary one.
