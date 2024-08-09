//////////////////////////////////////////////////////////////////////////////////////
// Copyright 2024 coScene
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//////////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <cobridge/base64.hpp>

TEST(Base64Test, EncodingTest) {
    constexpr char arr[] = {'A', 'B', 'C', 'D'};
    const std::string_view sv(arr, sizeof(arr));
    const std::string b64encoded = cobridge::base64Encode(sv);
    EXPECT_EQ(b64encoded, "QUJDRA==");
}

TEST(Base64Test, DecodeTest) {
    const std::vector<unsigned char> expectedVal = {0x00, 0xFF, 0x01, 0xFE};
    EXPECT_EQ(cobridge::base64Decode("AP8B/g=="), expectedVal);
}

TEST(Base64Test, DecodeInvalidStringTest) {
    // String length not multiple of 4
    EXPECT_THROW(cobridge::base64Decode("faefa"), std::runtime_error);
    // Invalid characters
    EXPECT_THROW(cobridge::base64Decode("fa^ef a"), std::runtime_error);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}