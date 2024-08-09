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

#include <cobridge/serialization.hpp>

TEST(SerializationTest, ServiceRequestSerialization) {
    foxglove::ServiceRequest req;
    req.serviceId = 2;
    req.callId = 1;
    req.encoding = "json";
    req.data = {1, 2, 3};

    std::vector<uint8_t> data(req.size());
    req.write(data.data());

    foxglove::ServiceRequest req2;
    req2.read(data.data(), data.size());
    EXPECT_EQ(req.serviceId, req2.serviceId);
    EXPECT_EQ(req.callId, req2.callId);
    EXPECT_EQ(req.encoding, req2.encoding);
    EXPECT_EQ(req.data.size(), req2.data.size());
    EXPECT_EQ(req.data, req2.data);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}