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

#include <gtest/gtest.h>
#include <serialization.hpp>
#include <common.hpp>

#include <cstring>
#include <string>
#include <vector>

namespace cobridge_base
{

// Helper function to write uint32 in little endian
void write_uint32_le(uint8_t * buffer, uint32_t value)
{
  buffer[0] = value & 0xFF;
  buffer[1] = (value >> 8) & 0xFF;
  buffer[2] = (value >> 16) & 0xFF;
  buffer[3] = (value >> 24) & 0xFF;
}

// Test fixture for ServiceResponse read tests
class ServiceResponseReadTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    // Initialize test data
  }

  void TearDown() override
  {
    // Cleanup
  }

  // Helper to create valid message
  std::vector<uint8_t> createValidMessage(
    uint32_t service_id = 100,
    uint32_t call_id = 200,
    const std::string & encoding = "cdr",
    const std::vector<uint8_t> & payload = {0x01, 0x02, 0x03})
  {
    std::vector<uint8_t> message;
    message.resize(12 + encoding.size() + payload.size());

    size_t offset = 0;

    // Service ID
    write_uint32_le(&message[offset], service_id);
    offset += 4;

    // Call ID
    write_uint32_le(&message[offset], call_id);
    offset += 4;

    // Encoding length
    write_uint32_le(&message[offset], static_cast<uint32_t>(encoding.size()));
    offset += 4;

    // Encoding string
    std::memcpy(&message[offset], encoding.data(), encoding.size());
    offset += encoding.size();

    // Payload
    std::memcpy(&message[offset], payload.data(), payload.size());

    return message;
  }
};

// Test: Valid message parsing
// cppcheck-suppress syntaxError
TEST_F(ServiceResponseReadTest, ValidMessageParsing)
{
  auto message = createValidMessage(123, 456, "cdr", {0xAA, 0xBB, 0xCC});

  ServiceResponse response;
  EXPECT_NO_THROW(response.read(message.data(), message.size()));

  EXPECT_EQ(response.service_id, 123u);
  EXPECT_EQ(response.call_id, 456u);
  EXPECT_EQ(response.encoding, "cdr");
  ASSERT_EQ(response.serv_data.size(), 3u);
  EXPECT_EQ(response.serv_data[0], 0xAA);
  EXPECT_EQ(response.serv_data[1], 0xBB);
  EXPECT_EQ(response.serv_data[2], 0xCC);
}

// Test: Empty payload is valid
TEST_F(ServiceResponseReadTest, EmptyPayload)
{
  auto message = createValidMessage(1, 2, "cdr", {});

  ServiceResponse response;
  EXPECT_NO_THROW(response.read(message.data(), message.size()));

  EXPECT_EQ(response.service_id, 1u);
  EXPECT_EQ(response.call_id, 2u);
  EXPECT_EQ(response.encoding, "cdr");
  EXPECT_TRUE(response.serv_data.empty());
}

// Test: Different encoding strings
TEST_F(ServiceResponseReadTest, DifferentEncodings)
{
  std::vector<std::string> encodings = {"cdr", "json", "protobuf", "msgpack"};

  for (const auto & enc : encodings) {
    auto message = createValidMessage(1, 1, enc, {0x01});
    ServiceResponse response;
    EXPECT_NO_THROW(response.read(message.data(), message.size()));
    EXPECT_EQ(response.encoding, enc);
  }
}

// Test: Message too short (less than 12 bytes)
TEST_F(ServiceResponseReadTest, MessageTooShort)
{
  // Test with various short lengths
  for (size_t len = 0; len < 12; ++len) {
    std::vector<uint8_t> message(len, 0x00);
    ServiceResponse response;

    EXPECT_THROW(
      {
        response.read(message.data(), message.size());
      },
      std::runtime_error) << "Expected exception for message length " << len;
  }
}

// Test: Encoding length exceeds MAX_ENCODING_LENGTH (256)
TEST_F(ServiceResponseReadTest, EncodingLengthTooLarge)
{
  std::vector<uint8_t> message(12);

  // Service ID and Call ID
  write_uint32_le(&message[0], 1);
  write_uint32_le(&message[4], 1);

  // Encoding length = 257 (exceeds MAX_ENCODING_LENGTH)
  write_uint32_le(&message[8], 257);

  ServiceResponse response;
  EXPECT_THROW(
    {
      response.read(message.data(), message.size());
    },
    std::runtime_error);
}

// Test: Encoding length at boundary (256 bytes - should pass)
TEST_F(ServiceResponseReadTest, EncodingLengthAtMaxBoundary)
{
  std::string long_encoding(256, 'x');
  auto message = createValidMessage(1, 1, long_encoding, {0x01});

  ServiceResponse response;
  EXPECT_NO_THROW(response.read(message.data(), message.size()));
  EXPECT_EQ(response.encoding.size(), 256u);
}

// Test: Encoding length exceeds remaining data
TEST_F(ServiceResponseReadTest, EncodingLengthExceedsData)
{
  std::vector<uint8_t> message(20);  // Total 20 bytes

  // Service ID and Call ID
  write_uint32_le(&message[0], 1);
  write_uint32_le(&message[4], 1);

  // Encoding length = 20 (but only 8 bytes remaining after header)
  write_uint32_le(&message[8], 20);

  ServiceResponse response;
  EXPECT_THROW(
    {
      response.read(message.data(), message.size());
    },
    std::runtime_error);
}

// Test: Exact fit (encoding fills all remaining space, no payload)
TEST_F(ServiceResponseReadTest, ExactFitNoPayload)
{
  auto message = createValidMessage(1, 1, "test", {});  // Exactly 12 + 4 = 16 bytes

  ServiceResponse response;
  EXPECT_NO_THROW(response.read(message.data(), message.size()));
  EXPECT_EQ(response.encoding, "test");
  EXPECT_TRUE(response.serv_data.empty());
}

// Test: Large payload
TEST_F(ServiceResponseReadTest, LargePayload)
{
  std::vector<uint8_t> large_payload(10000, 0xAB);
  auto message = createValidMessage(1, 1, "cdr", large_payload);

  ServiceResponse response;
  EXPECT_NO_THROW(response.read(message.data(), message.size()));
  EXPECT_EQ(response.serv_data.size(), 10000u);
  EXPECT_EQ(response.serv_data[0], 0xAB);
  EXPECT_EQ(response.serv_data[9999], 0xAB);
}

// Test: Zero-length encoding string
TEST_F(ServiceResponseReadTest, ZeroLengthEncoding)
{
  auto message = createValidMessage(1, 1, "", {0x01, 0x02});

  ServiceResponse response;
  EXPECT_NO_THROW(response.read(message.data(), message.size()));
  EXPECT_TRUE(response.encoding.empty());
  EXPECT_EQ(response.serv_data.size(), 2u);
}

// Test: Endianness - verify little endian reading
TEST_F(ServiceResponseReadTest, LittleEndianReading)
{
  std::vector<uint8_t> message(16);

  // Service ID = 0x12345678 in little endian: 0x78 0x56 0x34 0x12
  message[0] = 0x78;
  message[1] = 0x56;
  message[2] = 0x34;
  message[3] = 0x12;

  // Call ID = 0xABCDEF01 in little endian: 0x01 0xEF 0xCD 0xAB
  message[4] = 0x01;
  message[5] = 0xEF;
  message[6] = 0xCD;
  message[7] = 0xAB;

  // Encoding length = 0 (no encoding string)
  message[8] = 0x00;
  message[9] = 0x00;
  message[10] = 0x00;
  message[11] = 0x00;

  // Payload
  message[12] = 0xAA;
  message[13] = 0xBB;
  message[14] = 0xCC;
  message[15] = 0xDD;

  ServiceResponse response;
  EXPECT_NO_THROW(response.read(message.data(), message.size()));

  EXPECT_EQ(response.service_id, 0x12345678u);
  EXPECT_EQ(response.call_id, 0xABCDEF01u);
  EXPECT_TRUE(response.encoding.empty());
  EXPECT_EQ(response.serv_data.size(), 4u);
}

// Test: The actual bug case - Big Endian encoding_length = 3 read as Little Endian
TEST_F(ServiceResponseReadTest, EndiannessButActualCase)
{
  std::vector<uint8_t> message(20);

  // Service ID = 13 in little endian
  write_uint32_le(&message[0], 13);

  // Call ID = 1 in little endian
  write_uint32_le(&message[4], 1);

  // If someone sends 3 in BIG ENDIAN by mistake: 0x00 0x00 0x00 0x03
  // Read as little endian becomes: 0x03000000 = 50331648
  message[8] = 0x00;   // This is what happens with Big Endian encoding
  message[9] = 0x00;
  message[10] = 0x00;
  message[11] = 0x03;  // Value 3 in big endian

  ServiceResponse response;

  // Should throw because 50331648 > MAX_ENCODING_LENGTH (256)
  EXPECT_THROW(
    {
      response.read(message.data(), message.size());
    },
    std::runtime_error);
}

}  // namespace cobridge_base

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
