#pragma once

#include <iostream>

namespace media_transcode::test {

struct TestContext {
    int failures = 0;

    void expect(bool condition, const char* expression, const char* file, int line)
    {
        if (condition) {
            return;
        }

        ++failures;
        std::cerr << file << ':' << line << ": expectation failed: " << expression << '\n';
    }
};

} // namespace media_transcode::test

#define EXPECT_TRUE(ctx, expr) (ctx).expect(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define EXPECT_FALSE(ctx, expr) (ctx).expect(!static_cast<bool>(expr), "!(" #expr ")", __FILE__, __LINE__)
#define EXPECT_EQ(ctx, lhs, rhs) (ctx).expect(((lhs) == (rhs)), #lhs " == " #rhs, __FILE__, __LINE__)
#define EXPECT_NEAR(ctx, lhs, rhs, tolerance) \
    (ctx).expect((((lhs) >= ((rhs) - (tolerance))) && ((lhs) <= ((rhs) + (tolerance)))), \
                 #lhs " ~= " #rhs, __FILE__, __LINE__)
