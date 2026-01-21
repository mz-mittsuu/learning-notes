#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "alarm/alarm.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrEq;

class MockWriter : public IFileWriter {
public:
    MOCK_METHOD(bool, writeFile, (const std::string& path, const std::string& value), (override));
};

static const std::string LED_DIR = "/sys/class/leds/am62-sk:green:heartbeat/";

TEST(SensoralarmTest, Temp39_Off)
{
    auto mw = std::make_shared<MockWriter>();
    Sensoralarm alarm(mw);

    {
        InSequence s;
        EXPECT_CALL(*mw, writeFile(LED_DIR + "trigger", StrEq("none"))).WillOnce(Return(true));
        EXPECT_CALL(*mw, writeFile(LED_DIR + "brightness", StrEq("0"))).WillOnce(Return(true));
    }

    alarm.updateSensortemp(39);
    EXPECT_EQ(alarm.getLedStatus(), 0);
    EXPECT_EQ(alarm.getSensorTemp(), 39);
}

TEST(SensoralarmTest, Temp40_SlowBlink)
{
    auto mw = std::make_shared<MockWriter>();
    Sensoralarm alarm(mw);

    {
        InSequence s;
        EXPECT_CALL(*mw, writeFile(LED_DIR + "trigger", StrEq("timer"))).WillOnce(Return(true));
        EXPECT_CALL(*mw, writeFile(LED_DIR + "delay_on", StrEq("200"))).WillOnce(Return(true));
        EXPECT_CALL(*mw, writeFile(LED_DIR + "delay_off", StrEq("800"))).WillOnce(Return(true));
    }

    alarm.updateSensortemp(40);
    EXPECT_EQ(alarm.getLedStatus(), 1);
}

TEST(SensoralarmTest, Temp60_FastBlink)
{
    auto mw = std::make_shared<MockWriter>();
    Sensoralarm alarm(mw);

    {
        InSequence s;
        EXPECT_CALL(*mw, writeFile(LED_DIR + "trigger", StrEq("timer"))).WillOnce(Return(true));
        EXPECT_CALL(*mw, writeFile(LED_DIR + "delay_on", StrEq("100"))).WillOnce(Return(true));
        EXPECT_CALL(*mw, writeFile(LED_DIR + "delay_off", StrEq("100"))).WillOnce(Return(true));
    }

    alarm.updateSensortemp(60);
    EXPECT_EQ(alarm.getLedStatus(), 2);
}

TEST(SensoralarmTest, Temp80_On)
{
    auto mw = std::make_shared<MockWriter>();
    Sensoralarm alarm(mw);

    {
        InSequence s;
        EXPECT_CALL(*mw, writeFile(LED_DIR + "trigger", StrEq("none"))).WillOnce(Return(true));
        EXPECT_CALL(*mw, writeFile(LED_DIR + "brightness", StrEq("1"))).WillOnce(Return(true));
    }

    alarm.updateSensortemp(80);
    EXPECT_EQ(alarm.getLedStatus(), 3);
}

// 境界値は「状態判定だけ」確認（I/Oの細かい順序までは縛らない）
class SensoralarmBoundaryTest : public ::testing::TestWithParam<std::pair<int,int>> {};

TEST_P(SensoralarmBoundaryTest, LedStatusMatches)
{
    auto mw = std::make_shared<MockWriter>();
    Sensoralarm alarm(mw);

    EXPECT_CALL(*mw, writeFile(_, _)).WillRepeatedly(Return(true));

    auto [temp, expected] = GetParam();
    alarm.updateSensortemp(temp);
    EXPECT_EQ(alarm.getLedStatus(), expected);
}

INSTANTIATE_TEST_SUITE_P(
    Boundaries,
    SensoralarmBoundaryTest,
    ::testing::Values(
        std::make_pair(39,0),
        std::make_pair(40,1),
        std::make_pair(59,1),
        std::make_pair(60,2),
        std::make_pair(79,2),
        std::make_pair(80,3)
    )
);
