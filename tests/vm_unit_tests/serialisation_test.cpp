#include "../../src/Utils/util.h"
#include "../../src/WARDuino.h"
#include "gtest/gtest.h"

class SerialisationFixture : public ::testing::Test {
   protected:
    StackValue i32sv{};
    StackValue f32sv{};
    StackValue i64sv{};
    StackValue f64sv{};
    std::map<uint8_t, uint8_t> typesMap{{I32, 0}, {I64, 1}, {F32, 2}, {F64, 3}};

    std::vector<uint8_t*> conversionsToFree{};
    std::vector<StackValue*> svToFree{};

    ~SerialisationFixture() override {}

    void SetUp() override {
        // init whole values to 0
        this->i32sv.value.uint64 = 0;
        this->f32sv.value.uint64 = 0;
        this->i64sv.value.uint64 = 0;
        this->f64sv.value.uint64 = 0;

        this->i32sv.value_type = I32;
        this->f32sv.value_type = F32;
        this->i64sv.value_type = I64;
        this->f64sv.value_type = F64;
    }

    void TearDown() override {
        for (int i = 0; i < this->conversionsToFree.size(); i++) {
            delete this->conversionsToFree[i];
        }
        this->conversionsToFree.clear();
        for (int i = 0; i < this->svToFree.size(); i++) {
            delete this->svToFree[i];
        }
        this->svToFree.clear();
    }

    StackValue* newStackValue() {
        auto sv = new StackValue{};
        this->svToFree.push_back(sv);
        return sv;
    }

    uint8_t* serialiseU32(uint32_t value, bool includeType) {
        if (includeType) {
            uint8_t typeIdx = this->typesMap[I32];
            return this->integerToLEB128(value, &typeIdx);
        } else {
            return this->integerToLEB128(value);
        }
    }

    uint8_t* serialiseI32(int64_t value, bool includeType) {
        if (includeType) {
            uint8_t typeIdx = this->typesMap[I32];
            return this->integerToLEB128(value, &typeIdx);
        } else {
            return this->integerToLEB128(value);
        }
    }

    uint8_t* serialiseF32(float f, bool includeType) {
        if (includeType) {
            uint8_t typeIdx = this->typesMap[F32];
            return this->floatCopy(f, &typeIdx);
        } else {
            return this->floatCopy(f);
        }
    }

    uint8_t* serialiseF64(double f, bool includeType) {
        if (includeType) {
            uint8_t typeIdx = this->typesMap[F64];
            return this->doubleCopy(f, &typeIdx);
        } else {
            return this->doubleCopy(f);
        }
    }

    uint8_t* integerToLEB128(int64_t value, uint8_t* putInFront = nullptr) {
        std::vector<uint8_t> buffer;
        bool more = true;
        while (more) {
            uint8_t byte = value & 0x7F;
            value >>= 7;
            if ((value == 0 && (byte & 0x40) == 0) ||
                (value == -1 && (byte & 0x40) != 0)) {
                more = false;
            } else {
                byte |= 0x80;
            }
            buffer.push_back(byte);
        }
        if (putInFront != nullptr) {
            buffer.insert(buffer.begin(), *putInFront);
        }
        uint8_t* result = new uint8_t[buffer.size()];
        for (size_t i = 0; i < buffer.size(); ++i) {
            result[i] = buffer[i];
        }
        this->conversionsToFree.push_back(result);
        return result;
    }

    uint8_t* floatCopy(float f, uint8_t* putInFront = nullptr) {
        size_t size = sizeof(float);
        uint8_t* cpy = nullptr;
        uint8_t offset = 0;
        if (putInFront != nullptr) {
            size += 1;
            offset = 1;
        }

        cpy = new uint8_t[size];
        std::memcpy(cpy + offset, &f, sizeof(float));
        if (putInFront != nullptr) {
            cpy[0] = *putInFront;
        }

        this->conversionsToFree.push_back(cpy);
        return cpy;
    }

    uint8_t* doubleCopy(double d, uint8_t* putInFront = nullptr) {
        size_t size = sizeof(double);
        uint8_t* cpy = nullptr;
        uint8_t offset = 0;
        if (putInFront != nullptr) {
            size += 1;
            offset = 1;
        }

        cpy = new uint8_t[size];
        std::memcpy(cpy + offset, &d, sizeof(double));
        if (putInFront != nullptr) {
            cpy[0] = *putInFront;
        }

        this->conversionsToFree.push_back(cpy);
        return cpy;
    }
};

TEST_F(SerialisationFixture, DeserialisePositiveU32StackValueAndIgnoreType) {
    const bool includeType = false;
    int64_t newValue = 127;
    uint8_t* conversion = this->serialiseU32(newValue, includeType);

    bool successful =
        deserialiseStackValue(conversion, includeType, &this->i32sv);
    ASSERT_TRUE(successful) << "Deserialisation should be successful";
    EXPECT_EQ(this->i32sv.value_type, I32)
        << "Deserialisation should preserve type. ";
    EXPECT_EQ(this->i32sv.value.uint32, newValue)
        << "Deserialisation value does not match expected value";
}

TEST_F(SerialisationFixture, DeserialisePositiveU32StackValue) {
    const bool includeType = true;
    int64_t newValue = 127;
    uint8_t* conversion = this->serialiseU32(newValue, includeType);
    StackValue* freshSV = this->newStackValue();

    bool successful = deserialiseStackValue(conversion, includeType, freshSV);

    ASSERT_TRUE(successful) << "Deserialisation should be successful";
    EXPECT_EQ(freshSV->value_type, I32)
        << "Deserialisation should deserialise value type. ";
    EXPECT_EQ(freshSV->value.uint32, newValue)
        << "Deserialisation value does not match expected value";
}

TEST_F(SerialisationFixture, DeserialiseNegativeI32StackValueAndIgnoreType) {
    const bool includeType = false;
    int64_t newValue = -33;
    uint8_t* conversion = this->serialiseI32(newValue, includeType);

    bool successful =
        deserialiseStackValue(conversion, includeType, &this->i32sv);
    ASSERT_TRUE(successful) << "Deserialisation should be successful";
    EXPECT_EQ(this->i32sv.value_type, I32)
        << "Deserialisation should preserve type. ";
    EXPECT_EQ(this->i32sv.value.int32, newValue)
        << "Deserialisation value does not match expected value";
}

TEST_F(SerialisationFixture, DeserialiseNegativeI32StackValue) {
    const bool includeType = true;
    int64_t newValue = -33;
    uint8_t* conversion = this->serialiseI32(newValue, includeType);
    StackValue* freshSV = this->newStackValue();
    bool successful = deserialiseStackValue(conversion, includeType, freshSV);

    ASSERT_TRUE(successful) << "Deserialisation should be successful";
    EXPECT_EQ(freshSV->value_type, I32)
        << "Deserialisation should deserialise value type";
    EXPECT_EQ(freshSV->value.int32, newValue)
        << "Deserialisation value does not match expected value";
}

TEST_F(SerialisationFixture, DeserialisePositiveF32StackValueAndIgnoreType) {
    const bool includeType = false;
    float newValue{33.802};
    uint8_t* conversion = this->serialiseF32(newValue, includeType);

    bool successful =
        deserialiseStackValue(conversion, includeType, &this->f32sv);
    ASSERT_TRUE(successful) << "Deserialisation should be successful";
    EXPECT_EQ(this->f32sv.value_type, F32)
        << "Deserialisation should preserve type. ";
    EXPECT_EQ(this->f32sv.value.f32, newValue)
        << "Deserialisation value does not match expected value";
}

TEST_F(SerialisationFixture, DeserialisePositiveF32StackValue) {
    const bool includeType = true;
    float newValue{33.802};
    uint8_t* conversion = this->serialiseF32(newValue, includeType);
    StackValue* freshSV = this->newStackValue();
    bool successful = deserialiseStackValue(conversion, includeType, freshSV);

    ASSERT_TRUE(successful) << "Deserialisation should be successful";
    EXPECT_EQ(freshSV->value_type, F32)
        << "Deserialisation should deserialise value type";
    EXPECT_EQ(freshSV->value.f32, newValue)
        << "Deserialisation value does not match expected value";
}

TEST_F(SerialisationFixture, DeserialiseNegativeF32StackValueAndIgnoreType) {
    const bool includeType = false;
    float newValue{-33.802};
    uint8_t* conversion = this->serialiseF32(newValue, includeType);

    bool successful =
        deserialiseStackValue(conversion, includeType, &this->f32sv);
    ASSERT_TRUE(successful) << "Deserialisation should be successful";
    EXPECT_EQ(this->f32sv.value_type, F32)
        << "Deserialisation should preserve type. ";
    EXPECT_EQ(this->f32sv.value.f32, newValue)
        << "Deserialisation value does not match expected value";
}

TEST_F(SerialisationFixture, DeserialiseNegativeF32StackValue) {
    const bool includeType = true;
    float newValue{-33.802};
    StackValue* freshSV = this->newStackValue();
    uint8_t* conversion = this->serialiseF32(newValue, includeType);
    bool successful = deserialiseStackValue(conversion, includeType, freshSV);

    ASSERT_TRUE(successful) << "Deserialisation should be successful";
    EXPECT_EQ(freshSV->value_type, F32)
        << "Deserialisation should deserialise value type";
    EXPECT_EQ(freshSV->value.f32, newValue)
        << "Deserialisation value does not match expected value";
}

TEST_F(SerialisationFixture, DeserialisePositiveF64StackValueAndIgnoreType) {
    const bool includeType = false;
    double newValue{33.8020001};

    uint8_t* conversion = this->serialiseF64(newValue, includeType);

    bool successful =
        deserialiseStackValue(conversion, includeType, &this->f64sv);
    ASSERT_TRUE(successful) << "Deserialisation should be successful";
    EXPECT_EQ(this->f64sv.value_type, F64)
        << "Deserialisation should preserve type. ";
    EXPECT_EQ(this->f64sv.value.f64, newValue)
        << "Deserialisation value does not match expected value";
}

TEST_F(SerialisationFixture, DeserialisePositiveF64StackValue) {
    const bool includeType = true;
    double newValue{33.8020001};
    StackValue* freshSV = this->newStackValue();
    uint8_t* conversion = this->serialiseF64(newValue, includeType);

    bool successful = deserialiseStackValue(conversion, includeType, freshSV);
    ASSERT_TRUE(successful) << "Deserialisation should be successful";
    EXPECT_EQ(freshSV->value_type, F64)
        << "Deserialisation should deserialise value type";
    EXPECT_EQ(freshSV->value.f64, newValue)
        << "Deserialisation value does not match expected value";
}

TEST_F(SerialisationFixture, DeserialiseNegativeF64StackValueAndIgnoreType) {
    const bool includeType = false;
    double newValue{-33.8020001};
    uint8_t* conversion = this->serialiseF64(newValue, includeType);
    bool successful =
        deserialiseStackValue(conversion, includeType, &this->f64sv);

    ASSERT_TRUE(successful) << "Deserialisation should be successful";
    EXPECT_EQ(this->f64sv.value_type, F64)
        << "Deserialisation should preserve type. ";
    EXPECT_EQ(this->f64sv.value.f64, newValue)
        << "Deserialisation value does not match expected value";
}

TEST_F(SerialisationFixture, DeserialiseNegativeF64StackValue) {
    const bool includeType = true;
    double newValue{-33.8020001};

    StackValue* freshSV = this->newStackValue();
    uint8_t* conversion = this->serialiseF64(newValue, includeType);

    bool successful = deserialiseStackValue(conversion, includeType, freshSV);
    ASSERT_TRUE(successful) << "Deserialisation should be successful";
    EXPECT_EQ(freshSV->value_type, F64)
        << "Deserialisation should deserialise value type";
    EXPECT_EQ(freshSV->value.f64, newValue)
        << "Deserialisation value does not match expected value";
}

TEST_F(SerialisationFixture, DeserialiseInvalidType) {
    const bool includeType = true;
    uint8_t invalidType = 23;

    double newValue{23};

    StackValue* freshSV = this->newStackValue();
    uint8_t* conversion = this->serialiseF64(newValue, includeType);

    // change type
    conversion[0] = invalidType;

    bool successful = deserialiseStackValue(conversion, includeType, freshSV);
    ASSERT_FALSE(successful) << "Deserialisation should fail for "
                                "an invalid type";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}