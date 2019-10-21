#include "software/new_geom/point.h"

#include <gtest/gtest.h>

TEST(CreatePointTests, point_default_constructor_test)
{
    Point p = Point();
    EXPECT_EQ(0, p.x());
    EXPECT_EQ(0, p.y());
}

TEST(CreatePointTests, point_from_angle_test)
{
    Point p = Point::createFromAngle(Angle::ofDegrees(60));
    EXPECT_DOUBLE_EQ(0.5, p.x());
    EXPECT_DOUBLE_EQ(sqrt(3) / 2, p.y());
}

TEST(CreatePointTests, point_specific_constructor_test)
{
    Point p = Point(1, 2);
    EXPECT_EQ(1, p.x());
    EXPECT_EQ(2, p.y());
}

TEST(CreatePointTests, point_copy_constructor_test)
{
    Point p = Point(3, 4);
    Point q = Point(p);
    EXPECT_EQ(3, q.x());
    EXPECT_EQ(4, q.y());
}

TEST(CreatePointTests, point_from_vector_test)
{
    Vector v = Vector(2, 1);
    Point p  = Point(v);
    EXPECT_EQ(2, p.x());
    EXPECT_EQ(1, p.y());
}

TEST(SetPointTests, point_set_x_and_y_test)
{
    Point p = Point();
    p.setX(6);
    p.setY(5);
    EXPECT_EQ(6, p.x());
    EXPECT_EQ(5, p.y());

    p.set(2, 4);
    EXPECT_EQ(2, p.x());
    EXPECT_EQ(4, p.y());
}

TEST(PointLogicTests, point_dist_from_origin_test)
{
    Point p = Point(3, 4);
    EXPECT_EQ(5, p.distanceFromOrigin());
}

TEST(PointLogicTests, point_dist_from_other_point_test)
{
    Point p = Point(3, 4);
    Point q = Point(-3, -4);
    EXPECT_EQ(10, p.distanceFromPoint(q));
}

TEST(PointLogicTests, norm_vector_from_point_test)
{
    Point p  = Point(3, 4);
    Vector v = p.norm();
    EXPECT_EQ(0.6, v.x());
    EXPECT_EQ(0.8, v.y());
}

TEST(PointLogicTests, norm_vector_from_point_with_length_test)
{
    Point p  = Point(3, 4);
    Vector v = p.norm(2);
    EXPECT_EQ(1.2, v.x());
    EXPECT_EQ(1.6, v.y());
}

TEST(PointLogicTests, rotate_point_test)
{
    Point p = Point(3, 4);
    p       = p.rotate(Angle::quarter());
    EXPECT_DOUBLE_EQ(-4, p.x());
    EXPECT_DOUBLE_EQ(3, p.y());
    p = p.rotate(Angle::half());
    EXPECT_DOUBLE_EQ(4, p.x());
    EXPECT_DOUBLE_EQ(-3, p.y());
    p = p.rotate(Angle::quarter());
    EXPECT_DOUBLE_EQ(3, p.x());
    EXPECT_DOUBLE_EQ(4, p.y());
}

TEST(PointLogicTests, point_orientation_test)
{
    Point p = Point();
    EXPECT_EQ(Angle::zero(), p.orientation());
    p = p.rotate(Angle::half());
    EXPECT_EQ(Angle::half(), p.orientation());
}

TEST(PointLogicTests, point_is_nan_test)
{
    Point p = Point();
    EXPECT_FALSE(p.isnan());
    p.setX(NAN);
    EXPECT_TRUE(p.isnan());
    p = Point();
    EXPECT_FALSE(p.isnan());
    p.setY(NAN);
    EXPECT_TRUE(p.isnan());
}

TEST(PointLogicTests, point_is_close_test)
{
    Point p = Point();
    Point q = Point(1e-9 - 1e-10, 0);
    EXPECT_TRUE(p.isClose(q));
    q = Point(1, 0);
    EXPECT_FALSE(p.isClose(q));
    EXPECT_TRUE(p.isClose(q, 2));
}

TEST(PointOperatorTests, point_assignment_test)
{
    Point p = Point(2, 3);
    Point q = Point();
    q       = p;
    EXPECT_EQ(2, q.x());
    EXPECT_EQ(3, q.y());
}

TEST(PointOperatorTests, point_vector_sum_test)
{
    Point p  = Point(1, 1);
    Vector v = Vector(-2, -2);
    Point q  = p + v;
    EXPECT_EQ(-1, q.x());
    EXPECT_EQ(-1, q.y());
}

TEST(PointOperatorTests, point_vector_sum_set_test)
{
    Point p  = Point(-1, 1);
    Vector v = Vector(-2, -2);
    p += v;
    EXPECT_EQ(-3, p.x());
    EXPECT_EQ(-1, p.y());
}

TEST(PointOperatorTests, negate_point_test)
{
    Point p = Point(3, -5);
    Point q = -p;
    EXPECT_EQ(-3, q.x());
    EXPECT_EQ(5, q.y());
}

TEST(PointOperatorTests, point_equality_inequality_test)
{
    Point p = Point();
    Point q = Point(0, 0);
    EXPECT_EQ(p, q);

    q.setX(3);

    EXPECT_FALSE(p == q);
    EXPECT_TRUE(p != q);
}
