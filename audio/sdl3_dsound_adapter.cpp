/**
 * sdl3_dsound_adapter.cpp — see sdl3_dsound_adapter.h for the bridge's
 * rationale.
 */
#include "sdl3_dsound_adapter.h"

#ifndef _WIN32

#include "AudioChannel.h"
#include "sdl3_dsound.h"

#include <cstdarg>
#include <cstring>

namespace {

// Owns one sdl3_dsound.h IDirectSoundBuffer and forwards every method with a
// live caller to it. Destructor is protected (deletion only via Release(),
// matching the base interface's own "protected non-virtual destructor,
// COM-style refcounted release" idiom -- AudioChannel.h's own doc comment:
// "prevent accidental deletion through these borrowed interface views").
class Sdl3AudioDirectSoundBuffer final : public AudioDirectSoundBuffer {
public:
    explicit Sdl3AudioDirectSoundBuffer(IDirectSoundBuffer* impl) : impl_(impl) {}

    IDirectSoundBuffer* raw() { return impl_; }

    // Genuinely unused COM boilerplate -- no real caller in this tree.
    int32_t QueryInterface(void*, void**) override { return DSERR_INVALIDPARAM; }
    uint32_t AddRef() override { return 1; }
    int32_t GetCaps(void*) override { return DSERR_INVALIDPARAM; }
    int32_t GetCurrentPosition(uint32_t*, uint32_t*) override { return DSERR_INVALIDPARAM; }
    int32_t GetFormat(void*, uint32_t, uint32_t*) override { return DSERR_INVALIDPARAM; }
    int32_t GetVolume(int32_t*) override { return DSERR_INVALIDPARAM; }
    int32_t GetPan(int32_t*) override { return DSERR_INVALIDPARAM; }
    int32_t GetFrequency(uint32_t*) override { return DSERR_INVALIDPARAM; }
    int32_t Initialize(void*, void*) override { return DS_OK; }
    int32_t Restore() override { return DS_OK; }  // SDL3 buffers are never lost.

    uint32_t Release() override {
        delete this;
        return 0;
    }

    int32_t GetStatus(uint32_t* status) override { return impl_->GetStatus(status); }
    int32_t Lock(uint32_t offset, uint32_t bytes, void** region1, uint32_t* bytes1,
                 void** region2, uint32_t* bytes2, uint32_t flags) override {
        return impl_->Lock(offset, bytes, region1, bytes1, region2, bytes2, flags);
    }
    int32_t Play(uint32_t reserved1, uint32_t reserved2, uint32_t flags) override {
        return impl_->Play(reserved1, reserved2, flags);
    }
    int32_t SetCurrentPosition(uint32_t position) override {
        return impl_->SetCurrentPosition(position);
    }
    int32_t SetFormat(void* /*format*/) override {
        // NOTE: only ever called on the primary buffer (GameAudio::Init),
        // which this codebase never Plays/Locks -- see PROGRESS.md's
        // RESDATA/ResourceObject unification entry for the still-open
        // question of what real struct the original passes here (disassembly
        // shows a DIFFERENT stack buffer than the DSBUFFERDESC built at the
        // call site, almost certainly a WAVEFORMATEX). Safe no-op until that
        // resolves and this path gains a real caller.
        return DS_OK;
    }
    int32_t SetVolume(int32_t volume) override { return impl_->SetVolume(volume); }
    int32_t SetPan(int32_t pan) override { return impl_->SetPan(pan); }
    int32_t SetFrequency(uint32_t frequency) override { return impl_->SetFrequency(frequency); }
    int32_t Stop() override { return impl_->Stop(); }
    int32_t Unlock(void* region1, uint32_t bytes1, void* region2, uint32_t bytes2) override {
        return impl_->Unlock(region1, bytes1, region2, bytes2);
    }

protected:
    ~Sdl3AudioDirectSoundBuffer() { delete impl_; }

private:
    IDirectSoundBuffer* impl_;
};

class Sdl3AudioDirectSoundDevice final : public AudioDirectSoundDevice {
public:
    explicit Sdl3AudioDirectSoundDevice(IDirectSound* impl) : impl_(impl) {}

    int32_t QueryInterface(void*, void**) override { return DSERR_INVALIDPARAM; }
    uint32_t AddRef() override { return 1; }
    int32_t GetCaps(void*) override { return DSERR_INVALIDPARAM; }

    uint32_t Release() override {
        delete this;
        return 0;
    }

    // `description`/`buffer` are always really DSBUFFERDESC*/AudioDirectSoundBuffer**
    // (GameAudio.cpp's two real call sites); the trailing `...` mirrors the
    // original's own dual argument-count call shapes (both a fixed-3-arg and
    // an explicit-nullptr-third-arg form observed) -- exactly one `void*`
    // wave-data argument is always supplied by every real caller.
    int32_t CreateSoundBuffer(void* description, void* buffer, ...) override {
        if (description == nullptr || buffer == nullptr) {
            return DSERR_INVALIDPARAM;
        }
        va_list args;
        va_start(args, buffer);
        void* wave_data = va_arg(args, void*);
        va_end(args);

        IDirectSoundBuffer* out = nullptr;
        int32_t hr = impl_->CreateSoundBuffer(
            static_cast<DSBUFFERDESC*>(description), &out, wave_data);
        if (hr == DS_OK) {
            *static_cast<AudioDirectSoundBuffer**>(buffer) =
                new Sdl3AudioDirectSoundBuffer(out);
        }
        return hr;
    }

    // AudioChannel::LoadSound's real per-channel path: build a new playable
    // buffer sharing `source`'s format and a copy of its PCM data (matching
    // real DirectSound's IDirectSoundBuffer duplication semantics -- an
    // independent buffer with its own play position/volume/pan state).
    int32_t DuplicateSoundBuffer(void* source, void* copy) override {
        if (source == nullptr || copy == nullptr) {
            return DSERR_INVALIDPARAM;
        }
        auto* src = static_cast<Sdl3AudioDirectSoundBuffer*>(source);
        IDirectSoundBuffer* src_impl = src->raw();

        auto* dup_impl = new IDirectSoundBuffer();
        dup_impl->format = src_impl->format;
        dup_impl->data_len = src_impl->data_len;
        if (dup_impl->data_len > 0 && src_impl->audio_data != nullptr) {
            dup_impl->audio_data = new uint8_t[dup_impl->data_len];
            std::memcpy(dup_impl->audio_data, src_impl->audio_data, dup_impl->data_len);
        }
        *static_cast<AudioDirectSoundBuffer**>(copy) =
            new Sdl3AudioDirectSoundBuffer(dup_impl);
        return DS_OK;
    }

    int32_t SetCooperativeLevel(void* window, int32_t level) override {
        return impl_->SetCooperativeLevel(window, level);
    }

protected:
    ~Sdl3AudioDirectSoundDevice() { delete impl_; }

private:
    IDirectSound* impl_;
};

}  // namespace

AudioDirectSoundDevice* Sdl3CreateAudioDirectSoundDevice() {
    IDirectSound* impl = nullptr;
    if (DirectSoundCreate(nullptr, &impl, nullptr) != DS_OK || impl == nullptr) {
        return nullptr;
    }
    return new Sdl3AudioDirectSoundDevice(impl);
}

#endif  // !_WIN32
