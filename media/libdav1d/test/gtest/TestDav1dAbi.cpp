/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <bit>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>

#include <dav1d/dav1d.h>

#include "gtest/gtest.h"
#include "mozilla/ScopeExit.h"

namespace {

static_assert(std::is_same_v<decltype(Dav1dDataProps::offset), int64_t>);

constexpr int kCodedWidth = 32;
constexpr int kCodedHeight = 16;
constexpr int64_t kPacketOffset = (int64_t{1} << 40) + 17;

// MPL-2.0 fixture: a synthetic 8-bit I420 keyframe (Y=64, U=96, V=160)
// encoded losslessly with vendored libaom. AV1E_SET_RENDER_SIZE signals
// a 48x24 render size. aom_img_add_metadata adds a user-private type 6 OBU
// with payload {0x01}. The pinned revision of rav1d logs the type and
// accepts the packet.
constexpr uint8_t kKeyframe[] = {
    0x12, 0x00, 0x0a, 0x0a, 0x00, 0x00, 0x00, 0x02, 0x1f, 0xfd, 0xfb, 0xf9,
    0x80, 0x40, 0x2a, 0x03, 0x06, 0x01, 0x80, 0x32, 0x18, 0x10, 0x02, 0x00,
    0x5e, 0x00, 0x2e, 0x80, 0x00, 0x00, 0x1f, 0x7c, 0x92, 0x36, 0x07, 0x64,
    0x04, 0x97, 0x12, 0xb4, 0xea, 0x05, 0xa8, 0x48, 0x40,
};

// MPL-2.0 fixture: a synthetic 64x64 10-bit I420 keyframe (Y=512, U=384,
// V=640) encoded losslessly with vendored libaom and
// AV1E_SET_FILM_GRAIN_TEST_VECTOR=2 (overlapping grain blocks).
constexpr uint8_t kFilmGrainKeyframe[] = {
    0x12, 0x00, 0x0a, 0x0a, 0x00, 0x00, 0x00, 0x02, 0xaf, 0xff, 0xbf, 0x7f,
    0x38, 0x18, 0x32, 0x74, 0x10, 0x00, 0x80, 0x01, 0xb0, 0xaf, 0x20, 0x06,
    0x0f, 0xf6, 0x01, 0x00, 0x20, 0x7f, 0xa0, 0x10, 0x02, 0x07, 0xfa, 0x07,
    0xc2, 0x40, 0xc1, 0xc0, 0x40, 0xbe, 0xc4, 0x3e, 0xc3, 0xb4, 0xc0, 0xb3,
    0xc0, 0x3b, 0x43, 0x37, 0xbe, 0x5a, 0xd2, 0x42, 0xbd, 0xb7, 0xc4, 0x61,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x7f, 0xc0, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x7f, 0xa4, 0x06, 0x04, 0x02, 0x03, 0x02, 0x01, 0x00, 0x32, 0x32,
    0xf3, 0xb9, 0x00, 0x8a, 0x22, 0x5f, 0x31, 0xf6, 0x60, 0x87, 0xa6, 0xc8,
};

struct CallbackState {
  std::unique_ptr<uint8_t[]> mBuffer;
  std::string mLog;
};

int AllocatePicture(Dav1dPicture* aPicture, void* aCookie) {
  auto& state = *static_cast<CallbackState*>(aCookie);
  const auto& params = aPicture->p;
  if (state.mBuffer || params.layout != DAV1D_PIXEL_LAYOUT_I420 ||
      params.bpc != 8 || params.w != kCodedWidth || params.h != kCodedHeight) {
    return DAV1D_ERR(EINVAL);
  }

  // For this 8-bit picture, the allocator must pad both dimensions to
  // 128 pixels and add DAV1D_PICTURE_ALIGNMENT bytes at the end.
  constexpr size_t kPaddedDimension = 128;
  constexpr size_t kYBytes = kPaddedDimension * kPaddedDimension;
  constexpr size_t kUVBytes = kYBytes / 4;
  constexpr size_t kAlignment = DAV1D_PICTURE_ALIGNMENT;
  constexpr size_t kPictureBytes = kYBytes + 2 * kUVBytes + kAlignment;
  size_t capacity = kPictureBytes + kAlignment - 1;
  state.mBuffer = std::make_unique<uint8_t[]>(capacity);
  void* buffer = state.mBuffer.get();
  auto* y = static_cast<uint8_t*>(
      std::align(kAlignment, kPictureBytes, buffer, capacity));
  aPicture->data[0] = y;
  aPicture->data[1] = y + kYBytes;
  aPicture->data[2] = y + kYBytes + kUVBytes;
  aPicture->stride[0] = kPaddedDimension;
  aPicture->stride[1] = kPaddedDimension / 2;
  aPicture->allocator_data = state.mBuffer.get();
  return 0;
}

void ReleasePicture(Dav1dPicture* aPicture, void* aCookie) {
  auto& state = *static_cast<CallbackState*>(aCookie);
  EXPECT_TRUE(state.mBuffer);
  EXPECT_EQ(state.mBuffer.get(), aPicture->allocator_data);
  state.mBuffer.reset();
}

void CollectLog(void* aCookie, const char* aFormat, va_list aArgs) {
  char message[128];
  if (std::vsnprintf(message, sizeof(message), aFormat, aArgs) >= 0) {
    static_cast<CallbackState*>(aCookie)->mLog += message;
  }
}

TEST(Dav1dAbi, DefaultInloopFilters)
{
  Dav1dSettings settings;
  dav1d_default_settings(&settings);

  // An ABI mismatch can write a value outside the C++ enum's range.
  using FilterBits = std::underlying_type_t<Dav1dInloopFilterType>;
  EXPECT_EQ(static_cast<FilterBits>(DAV1D_INLOOPFILTER_ALL),
            std::bit_cast<FilterBits>(settings.inloop_filters));
}

TEST(Dav1dAbi, DecodeKeyframe)
{
  CallbackState state;
  Dav1dSettings settings;
  dav1d_default_settings(&settings);
  settings.n_threads = 1;
  settings.max_frame_delay = 1;
  settings.allocator = {&state, AllocatePicture, ReleasePicture};
  settings.logger = {&state, CollectLog};

  Dav1dContext* context = nullptr;
  ASSERT_EQ(0, dav1d_open(&context, &settings));
  auto close = mozilla::MakeScopeExit([&] { dav1d_close(&context); });

  Dav1dData data{};
  auto* bytes = dav1d_data_create(&data, sizeof(kKeyframe));
  ASSERT_NE(nullptr, bytes);
  auto unrefData = mozilla::MakeScopeExit([&] { dav1d_data_unref(&data); });
  std::memcpy(bytes, kKeyframe, sizeof(kKeyframe));
  data.m.offset = kPacketOffset;
  ASSERT_EQ(0, dav1d_send_data(context, &data)) << state.mLog;

  Dav1dPicture picture{};
  ASSERT_EQ(0, dav1d_get_picture(context, &picture)) << state.mLog;
  auto unrefPicture =
      mozilla::MakeScopeExit([&] { dav1d_picture_unref(&picture); });

  EXPECT_EQ(kCodedWidth, picture.p.w);
  EXPECT_EQ(kCodedHeight, picture.p.h);
  EXPECT_EQ(kPacketOffset, picture.m.offset);
  ASSERT_NE(nullptr, picture.frame_hdr);
  EXPECT_EQ(DAV1D_FRAME_TYPE_KEY, picture.frame_hdr->frame_type);
  EXPECT_EQ(1, picture.frame_hdr->have_render_size);
  EXPECT_EQ(48, picture.frame_hdr->render_width);
  EXPECT_EQ(24, picture.frame_hdr->render_height);

  ASSERT_TRUE(state.mBuffer);
  EXPECT_EQ(state.mBuffer.get(), picture.allocator_data);
  ASSERT_NE(nullptr, picture.data[0]);
  ASSERT_NE(nullptr, picture.data[1]);
  ASSERT_NE(nullptr, picture.data[2]);
  EXPECT_EQ(64, *static_cast<const uint8_t*>(picture.data[0]));
  EXPECT_EQ(96, *static_cast<const uint8_t*>(picture.data[1]));
  EXPECT_EQ(160, *static_cast<const uint8_t*>(picture.data[2]));

  dav1d_close(&context);
  close.release();
  EXPECT_TRUE(state.mBuffer);
  dav1d_picture_unref(&picture);
  unrefPicture.release();
  EXPECT_FALSE(state.mBuffer);
  EXPECT_NE(std::string::npos, state.mLog.find("Unknown Metadata OBU type 6"))
      << state.mLog;
}

TEST(Dav1dAbi, Decode10BitFilmGrain)
{
  constexpr int kFrameSize = 64;
  // Reference samples at the first block overlap, with grain off and on.
  constexpr uint16_t kCenterSamples[2][3] = {{512, 384, 640}, {510, 376, 637}};

  for (int applyGrain = 0; applyGrain < 2; ++applyGrain) {
    SCOPED_TRACE(testing::Message() << "apply_grain=" << applyGrain);
    Dav1dSettings settings;
    dav1d_default_settings(&settings);
    settings.n_threads = 1;
    settings.max_frame_delay = 1;
    settings.apply_grain = applyGrain;

    Dav1dContext* context = nullptr;
    ASSERT_EQ(0, dav1d_open(&context, &settings));
    auto close = mozilla::MakeScopeExit([&] { dav1d_close(&context); });

    Dav1dData data{};
    auto* bytes = dav1d_data_create(&data, sizeof(kFilmGrainKeyframe));
    ASSERT_NE(nullptr, bytes);
    auto unrefData = mozilla::MakeScopeExit([&] { dav1d_data_unref(&data); });
    std::memcpy(bytes, kFilmGrainKeyframe, sizeof(kFilmGrainKeyframe));
    ASSERT_EQ(0, dav1d_send_data(context, &data));

    Dav1dPicture picture{};
    ASSERT_EQ(0, dav1d_get_picture(context, &picture));
    auto unrefPicture =
        mozilla::MakeScopeExit([&] { dav1d_picture_unref(&picture); });

    ASSERT_EQ(10, picture.p.bpc);
    ASSERT_EQ(DAV1D_PIXEL_LAYOUT_I420, picture.p.layout);
    ASSERT_EQ(kFrameSize, picture.p.w);
    ASSERT_EQ(kFrameSize, picture.p.h);
    ASSERT_NE(nullptr, picture.seq_hdr);
    EXPECT_EQ(1, picture.seq_hdr->film_grain_present);
    ASSERT_NE(nullptr, picture.frame_hdr);
    const auto& grain = picture.frame_hdr->film_grain;
    EXPECT_EQ(1, grain.present);
    EXPECT_EQ(45231u, grain.data.seed);
    EXPECT_EQ(1, grain.data.overlap_flag);

    for (int plane = 0; plane < 3; ++plane) {
      SCOPED_TRACE(testing::Message() << "plane=" << plane);
      ASSERT_NE(nullptr, picture.data[plane]);
      const bool chroma = plane != 0;
      const int center = chroma ? kFrameSize / 4 : kFrameSize / 2;
      const auto* row = static_cast<const uint8_t*>(picture.data[plane]) +
                        center * picture.stride[chroma];
      EXPECT_EQ(kCenterSamples[applyGrain][plane],
                reinterpret_cast<const uint16_t*>(row)[center]);
    }
  }
}

}  // namespace
