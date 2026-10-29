"""Extension contract for future real native implementations.

There is no provider today. This validates source evidence before a future patch
assembler; it neither authorizes installation nor changes an image. No external
approval service, caller token or 'qualified' boolean exists.
"""
from dataclasses import dataclass
import hashlib
from typing import Protocol
from .config import OFFICIAL

@dataclass(frozen=True)
class Patch:
    offset: int
    expected: bytes
    replacement: bytes
    instruction_set: str
    abi_document: str
    import_symbols: tuple
    placement_evidence: str

@dataclass(frozen=True)
class Candidate:
    model: str
    version: str
    input_img_sha256: str
    patches: tuple
    supplied_imports: tuple
    boot_loader_evidence: str
    recovery_evidence: str

class PatchProvider(Protocol):
    def plan(self, frozen_config: bytes, original_img: bytes) -> Candidate: ...

def validate_candidate(candidate,original):
    if type(candidate) is not Candidate: raise ValueError('typed concrete candidate required')
    if (candidate.model,candidate.version,candidate.input_img_sha256)!=('M10','4.26.36.10',OFFICIAL['img_sha256']): raise ValueError('exact baseline required')
    if hashlib.sha256(original).hexdigest()!=candidate.input_img_sha256: raise ValueError('original IMG preimage mismatch')
    if not candidate.patches or not candidate.boot_loader_evidence or not candidate.recovery_evidence: raise ValueError('real loader/recovery and executable changes required')
    spans=[]
    for p in candidate.patches:
        if type(p) is not Patch or type(p.offset) is not int or p.offset<0 or not p.expected or len(p.expected)!=len(p.replacement): raise ValueError('bounded same-size patch/preimage required; relocation needs a different concrete implementation')
        if p.instruction_set not in ('Thumb1','ARM') or not p.abi_document or not p.placement_evidence: raise ValueError('instruction/ABI/placement evidence required')
        end=p.offset+len(p.expected)
        if end>len(original) or original[p.offset:end]!=p.expected: raise ValueError('exact instruction preimage mismatch')
        if not set(p.import_symbols)<=set(candidate.supplied_imports): raise ValueError('unresolved native imports')
        spans.append((p.offset,end))
    spans.sort()
    if any(a[1]>b[0] for a,b in zip(spans,spans[1:])): raise ValueError('patch overlap')
    return spans

# Developers add concrete providers and an assembler in source after implementing
# placement, load coverage, full ABI and recovery. This empty tuple is descriptive,
# not a permission registry and not bypassable by a config field.
PROVIDERS=()
