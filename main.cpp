#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <SDL.h>
#include <png.h>

template <typename T>
struct Rect
{
    T x, y, w, h;
};

using Rectf = Rect<float>;

SDL_Rect rectf_for_sdl(Rectf rect)
{
    return {(int) floorf(rect.x),
            (int) floorf(rect.y),
            (int) floorf(rect.w),
            (int) floorf(rect.h)};
}

template <typename T>
struct Vec2
{
    T x, y;
};

using Vec2f = Vec2<float>;

template <typename T> Vec2<T> vec2(T x, T y) { return {x, y}; }

template <typename T> Vec2<T> constexpr operator+(Vec2<T> a, Vec2<T> b) { return {a.x + b.x, a.y + b.y}; }
template <typename T> Vec2<T> constexpr operator*(Vec2<T> a, Vec2<T> b) { return {a.x * b.x, a.y * b.y}; }
template <typename T> Vec2<T> constexpr operator*(Vec2<T> a, T b) { return {a.x * b, a.y * b}; }
template <typename T> Vec2<T> constexpr &operator+=(Vec2<T> &a, Vec2<T> b) { a = a + b; return a; }

void sec(int code)
{
    if (code < 0) {
        fprintf(stderr, "SDL pooped itself: %s\n", SDL_GetError());
        abort();
    }
}

template <typename T>
T *sec(T *ptr)
{
    if (ptr == nullptr) {
        fprintf(stderr, "SDL pooped itself: %s\n", SDL_GetError());
        abort();
    }

    return ptr;
}

struct Sprite
{
    SDL_Rect srcrect;
    SDL_Texture *texture;
};

void render_sprite(SDL_Renderer *renderer,
                   Sprite texture,
                   Rectf destrect,
                   SDL_RendererFlip flip = SDL_FLIP_NONE)
{
    SDL_Rect rect = rectf_for_sdl(destrect);
    sec(SDL_RenderCopyEx(
            renderer,
            texture.texture,
            &texture.srcrect,
            &rect,
            0.0,
            nullptr,
            flip));
}

SDL_Surface *load_png_file_as_surface(const char *image_filename)
{
    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&image, image_filename)) {
        fprintf(stderr, "Could not read file `%s`: %s\n", image_filename, image.message);
        abort();
    }
    image.format = PNG_FORMAT_RGBA;
    uint32_t *image_pixels = new uint32_t[image.width * image.height];

    if (!png_image_finish_read(&image, nullptr, image_pixels, 0, nullptr)) {
        fprintf(stderr, "libpng pooped itself: %s\n", image.message);
        abort();
    }

    SDL_Surface* image_surface =
        sec(SDL_CreateRGBSurfaceFrom(image_pixels,
                                     (int) image.width,
                                     (int) image.height,
                                     32,
                                     (int) image.width * 4,
                                     0x000000FF,
                                     0x0000FF00,
                                     0x00FF0000,
                                     0xFF000000));
    return image_surface;
}

SDL_Texture *load_texture_from_png_file(SDL_Renderer *renderer,
                                        const char *image_filename)
{
    SDL_Surface *image_surface =
        load_png_file_as_surface(image_filename);

    SDL_Texture *image_texture =
        sec(SDL_CreateTextureFromSurface(renderer,
                                         image_surface));
    SDL_FreeSurface(image_surface);

    return image_texture;
}

Sprite load_png_file_as_sprite(SDL_Renderer *renderer, const char *image_filename)
{
    Sprite sprite = {};
    sprite.texture = load_texture_from_png_file(renderer, image_filename);
    sec(SDL_QueryTexture(sprite.texture, NULL, NULL, &sprite.srcrect.w, &sprite.srcrect.h));
    return sprite;
}

struct Sample_S16
{
    int16_t* audio_buf;
    Uint32 audio_len;
    Uint32 audio_cur;
};

const size_t SAMPLE_MIXER_CAPACITY = 5;

struct Sample_Mixer
{
    float volume;
    Sample_S16 samples[SAMPLE_MIXER_CAPACITY];

    void play_sample(Sample_S16 sample)
    {
        for (size_t i = 0; i < SAMPLE_MIXER_CAPACITY; ++i) {
            if (samples[i].audio_cur >= samples[i].audio_len) {
                samples[i] = sample;
                samples[i].audio_cur = 0;
                return;
            }
        }
    }
};

const size_t SOMETHING_SOUND_FREQ = 48000;
const size_t SOMETHING_SOUND_FORMAT = 32784;
const size_t SOMETHING_SOUND_CHANNELS = 1;
const size_t SOMETHING_SOUND_SAMPLES = 4096;

Sample_S16 load_wav_as_sample_s16(const char *file_path)
{
    Sample_S16 sample = {};
    SDL_AudioSpec want = {};
    if (SDL_LoadWAV(file_path, &want, (Uint8**) &sample.audio_buf, &sample.audio_len) == nullptr) {
        fprintf(stderr, "SDL pooped itself: Failed to load %s: %s\n",
                file_path, SDL_GetError());
        abort();
    }

    assert(SDL_AUDIO_BITSIZE(want.format) == 16);
    assert(SDL_AUDIO_ISLITTLEENDIAN(want.format));
    assert(SDL_AUDIO_ISSIGNED(want.format));
    assert(SDL_AUDIO_ISINT(want.format));
    assert(want.freq == SOMETHING_SOUND_FREQ);
    assert(want.channels == SOMETHING_SOUND_CHANNELS);
    assert(want.samples == SOMETHING_SOUND_SAMPLES);

    sample.audio_len /= 2;

    return sample;
}

void sample_mixer_audio_callback(void *userdata, Uint8 *stream, int len)
{
    Sample_Mixer *mixer = (Sample_Mixer *)userdata;

    int16_t *output = (int16_t *)stream;
    size_t output_len = (size_t) len / sizeof(*output);

    memset(stream, 0, (size_t) len);
    for (size_t i = 0; i < SAMPLE_MIXER_CAPACITY; ++i) {
        for (size_t j = 0; j < output_len; ++j) {
            int16_t x = 0;

            if (mixer->samples[i].audio_cur < mixer->samples[i].audio_len) {
                x = mixer->samples[i].audio_buf[mixer->samples[i].audio_cur];
                mixer->samples[i].audio_cur += 1;
            }

            output[j] = (int16_t) std::clamp(
                output[j] + x,
                (int) std::numeric_limits<int16_t>::min(),
                (int) std::numeric_limits<int16_t>::max());
        }
    }

    for (size_t i = 0; i < output_len; ++i) {
        output[i] = (int16_t) (output[i] * mixer->volume);
    }
}

struct Rubber_Animat
{
    float begin;
    float end;
    float duration;
    float t;

    Rectf transform_rect(Rectf texbox, Vec2f pos) const
    {
        const float offset = begin + (end - begin) * (t / duration);
        const float w = texbox.w + offset * texbox.h;
        const float h = texbox.h - offset * texbox.h;
        return {pos.x - w * 0.5f, pos.y + (texbox.h * 0.5f) - h, w, h};
    }

    void update(float dt)
    {
        if (!finished()) t += dt;
    }

    bool finished() const
    {
        return t >= duration;
    }

    void reset()
    {
        t = 0.0f;
    }
};

template <size_t N>
struct Compose_Rubber_Animat
{
    Rubber_Animat rubber_animats[N];
    size_t current;

    Rectf transform_rect(Rectf texbox, Vec2f pos) const
    {
        return rubber_animats[std::min(current, N - 1)].transform_rect(texbox, pos);
    }

    void update(float dt)
    {
        if (finished()) return;

        if (rubber_animats[current].finished()) {
            current += 1;
        }

        rubber_animats[current].update(dt);
    }

    bool finished() const
    {
        return current >= N;
    }

    void reset()
    {
        current = 0;
        for (size_t i = 0; i < N; ++i) {
            rubber_animats[i].reset();
        }
    }
};

int main(void)
{
    sec(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO));

    Sample_S16 jump_sample = load_wav_as_sample_s16("./qubodup-cfork-ccby3-jump.wav");
    Sample_Mixer mixer = {};
    mixer.volume = 0.2f;

    SDL_AudioSpec want = {};
    want.freq = SOMETHING_SOUND_FREQ;
    want.format = SOMETHING_SOUND_FORMAT;
    want.channels = SOMETHING_SOUND_CHANNELS;
    want.samples = SOMETHING_SOUND_SAMPLES;
    want.callback = sample_mixer_audio_callback;
    want.userdata = &mixer;

    SDL_AudioSpec have = {};
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(
        NULL,
        0,
        &want,
        &have,
        SDL_AUDIO_ALLOW_FORMAT_CHANGE);

    if (dev == 0) {
        fprintf(stderr, "SDL pooped itself: Failed to open audio: %s\n", SDL_GetError());
        abort();
    }

    if (have.format != want.format) {
        fprintf(stderr, "[WARN] We didn't get expected audio format.\n");
        abort();
    }
    SDL_PauseAudioDevice(dev, 0);

    SDL_Window *window =
        sec(SDL_CreateWindow(
                "KKona",
                0, 0, 800, 600,
                SDL_WINDOW_RESIZABLE));

    SDL_Renderer *renderer =
        sec(SDL_CreateRenderer(
                window, -1,
                SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED));

    auto kkona = load_png_file_as_sprite(renderer, "./KKona.png");

    Rubber_Animat prepare_animat = {};
    prepare_animat.begin = 0.0f;
    prepare_animat.end = 0.2f;
    prepare_animat.duration = 0.5f;

    enum Jump_Animat_Phase
    {
        ATTACK = 0,
        RECOVER,
        N
    };
    Compose_Rubber_Animat<N> jump_animat = {};
    jump_animat.rubber_animats[ATTACK].begin = 0.2f;
    jump_animat.rubber_animats[ATTACK].end = -0.2f;
    jump_animat.rubber_animats[ATTACK].duration = 0.1f;

    jump_animat.rubber_animats[RECOVER].begin = -0.2f;
    jump_animat.rubber_animats[RECOVER].end = 0.0f;
    jump_animat.rubber_animats[RECOVER].duration = 0.2f;

    // This is hackish
    jump_animat.current = N - 1;
    jump_animat.rubber_animats[N - 1].t = jump_animat.rubber_animats[N - 1].duration;

    bool jump = true;

    const auto TEXBOX_SIZE = 64.0f * 4.0f;
    const Rectf texbox_local = {
        - (TEXBOX_SIZE / 2), - (TEXBOX_SIZE / 2),
        TEXBOX_SIZE, TEXBOX_SIZE
    };

    const float FLOOR = 800.0f;
    Vec2f gravity = vec2(0.0f, 3000.0f);
    Vec2f position = vec2(500.0f, FLOOR);
    Vec2f velocity = vec2(0.0f, 0.0f);
    for(;;) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT: {
                exit(0);
            } break;

            case SDL_KEYDOWN: {
                switch (event.key.keysym.sym) {
                case SDLK_SPACE: {
                    if (!event.key.repeat) {
                        jump = false;
                        prepare_animat.reset();
                    }
                } break;
                }
            } break;

            case SDL_KEYUP: {
                switch (event.key.keysym.sym) {
                case SDLK_SPACE: {
                    if (!event.key.repeat) {
                        jump = true;
                        jump_animat.reset();

                        velocity.y = gravity.y * -0.5f;
                        mixer.play_sample(jump_sample);
                    }
                } break;
                }
            } break;
            }
        }

        sec(SDL_SetRenderDrawColor(renderer, 18, 8, 8, 255));
        sec(SDL_RenderClear(renderer));

        const float dt = 1.0f / 60.0f;
        velocity += gravity * dt;
        position += velocity * dt;

        if (position.y >= FLOOR) {
            velocity = vec2(0.0f, 0.0f);
            position.y = FLOOR;
        }

        if (jump) {
            render_sprite(renderer,
                          kkona,
                          jump_animat.transform_rect(texbox_local, position));
            jump_animat.update(dt);
        } else {
            render_sprite(renderer,
                          kkona,
                          prepare_animat.transform_rect(texbox_local, position));
            prepare_animat.update(dt);
        }

         SDL_RenderPresent(renderer);
    }
}
