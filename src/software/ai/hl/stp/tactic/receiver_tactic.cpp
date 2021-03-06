#include "software/ai/hl/stp/tactic/receiver_tactic.h"

#include <g3log/g3log.hpp>

#include "shared/constants.h"
#include "software/ai/hl/stp/action/move_action.h"
#include "software/ai/hl/stp/evaluation/calc_best_shot.h"
#include "software/geom/util.h"

using namespace Passing;
using namespace Evaluation;

ReceiverTactic::ReceiverTactic(const Field& field, const Team& friendly_team,
                               const Team& enemy_team, const Passing::Pass pass,
                               const Ball& ball, bool loop_forever)
    : field(field),
      pass(pass),
      ball(ball),
      friendly_team(friendly_team),
      enemy_team(enemy_team),
      Tactic(loop_forever)
{
}

std::string ReceiverTactic::getName() const
{
    return "Receiver Tactic";
}

void ReceiverTactic::updateParams(const Team& updated_friendly_team,
                                  const Team& updated_enemy_team,
                                  const Passing::Pass& updated_pass,
                                  const Ball& updated_ball)
{
    this->friendly_team = updated_friendly_team;
    this->enemy_team    = updated_enemy_team;
    this->ball          = updated_ball;
    this->pass          = updated_pass;
}

double ReceiverTactic::calculateRobotCost(const Robot& robot, const World& world)
{
    // Prefer robots closer to the pass receive position
    // We normalize with the total field length so that robots that are within the field
    // have a cost less than 1
    double cost =
        (robot.position() - pass.receiverPoint()).len() / world.field().totalLength();
    return std::clamp<double>(cost, 0, 1);
}

void ReceiverTactic::calculateNextIntent(IntentCoroutine::push_type& yield)
{
    MoveAction move_action =
        MoveAction(MoveAction::ROBOT_CLOSE_TO_DEST_THRESHOLD,
                   MoveAction::ROBOT_CLOSE_TO_ORIENTATION_THRESHOLD, true);

    // Setup for the pass. We want to use any free time before the pass starts putting
    // ourselves in the best position possible to take the pass
    // We wait for the ball to start moving at least a bit to make sure the passer
    // has actually started the pass
    while (ball.lastUpdateTimestamp() < pass.startTime() || ball.velocity().len() < 0.5)
    {
        // If there is a feasible shot we can take, we want to wait for the pass at the
        // halfway point between the angle required to receive the ball and the angle
        // for a one-time shot
        std::optional<std::pair<Point, Angle>> shot = findFeasibleShot();
        Angle desired_angle                         = pass.receiverOrientation();
        if (shot)
        {
            auto [target_position, _] = *shot;
            Angle shot_angle = (target_position - robot->position()).orientation();
            // If we do have a valid shot on net, orient the robot to face in between
            // the pass vector and shot vector, so the robot can quickly orient itself
            // to either receive the pass, or take the shot. Also, not directly facing
            // where we plan on kicking may throw off the enemy AI
            desired_angle = (shot_angle + pass.receiverOrientation()) / 2;
        }
        // We want the robot to move to the receiving position for the shot and also
        // rotate to the correct orientation
        yield(move_action.updateStateAndGetNextIntent(*robot, pass.receiverPoint(),
                                                      desired_angle, 0));
    }

    // Check if we can shoot on the enemy goal from the receiver position
    std::optional<std::pair<Point, Angle>> best_shot_opt =
        calcBestShotOnEnemyGoal(field, friendly_team, enemy_team, *robot);

    // Vector from the ball to the robot
    Vector ball_to_robot_vector = ball.position() - robot->position();
    std::optional<std::pair<Point, Angle>> best_shot = findFeasibleShot();
    if (best_shot)
    {
        LOG(DEBUG) << "Taking one-touch shot";
        auto [best_shot_target, _] = *best_shot;

        // The angle between the ball velocity and a vector from the ball to the robot
        Vector ball_velocity = ball.velocity();
        ball_to_robot_vector = robot->position() - ball.position();
        Angle ball_robot_angle =
            ball_velocity.orientation().minDiff(ball_to_robot_vector.orientation());

        // Keep trying to shoot the ball while it's traveling roughly towards the robot
        // (or moving slowly because we can't be certain of the velocity vector if it is)
        while (ball_robot_angle.abs() < Angle::ofDegrees(90) || ball_velocity.len() < 0.5)
        {
            auto [ideal_position, ideal_orientation] =
                getOneTimeShotPositionAndOrientation(*robot, ball, best_shot_target);

            yield(move_action.updateStateAndGetNextIntent(
                *robot, ideal_position, ideal_orientation, 0, false, false, AUTOKICK));

            // Calculations to check for termination conditions
            ball_to_robot_vector = robot->position() - ball.position();
            ball_robot_angle =
                ball_velocity.orientation().minDiff(ball_to_robot_vector.orientation());
        }
    }
    // If we can't shoot on the enemy goal, just try to receive the pass as cleanly as
    // possible
    else
    {
        LOG(DEBUG) << "Receiving and dribbling";
        while ((ball.position() - robot->position()).len() >
               DIST_TO_FRONT_OF_ROBOT_METERS + 2 * BALL_MAX_RADIUS_METERS)
        {
            Point ball_receive_pos = closestPointOnLine(
                robot->position(), ball.position(), ball.position() + ball.velocity());
            Angle ball_receive_orientation =
                (ball.position() - robot->position()).orientation();

            // Move into position with the dribbler on
            yield(move_action.updateStateAndGetNextIntent(
                *robot, ball_receive_pos, ball_receive_orientation, 0, true, NONE));
        }
    }
    LOG(DEBUG) << "Finished";
}

Angle ReceiverTactic::getOneTimeShotDirection(const Ray& shot, const Ball& ball)
{
    Vector shot_vector = shot.getDirection();
    Angle shot_dir     = shot.getDirection().orientation();

    Vector ball_vel    = ball.velocity();
    Vector lateral_vel = ball_vel.project(shot_vector.norm().perp());
    // The lateral speed is roughly a measure of the lateral velocity we need to
    // "cancel out" in order for our shot to go in the expected direction.
    // The scaling factor of 0.3 is a magic number that was carried over from the old
    // code. It seems to work well on the field.
    double lateral_speed = 0.3 * lateral_vel.len();
    // This kick speed is based off of the value used in the firmware `MovePrimitive` when
    // autokick is enabled
    double kick_speed = BALL_MAX_SPEED_METERS_PER_SECOND - 1;
    Angle shot_offset = Angle::asin(lateral_speed / kick_speed);

    // check which direction the ball is going in so we can decide which direction to
    // apply the offset in
    if (lateral_vel.dot(shot_vector.rotate(Angle::quarter())) > 0)
    {
        // need to go clockwise
        shot_offset = -shot_offset;
    }
    return shot_dir + shot_offset;
}

std::optional<std::pair<Point, Angle>> ReceiverTactic::findFeasibleShot()
{
    // Check if we can shoot on the enemy goal from the receiver position
    std::optional<std::pair<Point, Angle>> best_shot_opt =
        calcBestShotOnEnemyGoal(field, friendly_team, enemy_team, *robot);

    // Vector from the ball to the robot
    Vector robot_to_ball = ball.position() - robot->position();

    // The angle the robot will have to deflect the ball to shoot
    Angle abs_angle_between_pass_and_shot_vectors;
    // The percentage of open net the robot would shoot on
    double net_percent_open;
    if (best_shot_opt)
    {
        Vector robot_to_shot_target = best_shot_opt->first - robot->position();
        abs_angle_between_pass_and_shot_vectors =
            (robot_to_ball.orientation() - robot_to_shot_target.orientation())
                .angleMod()
                .abs();

        Angle goal_angle =
            acuteVertexAngle(field.friendlyGoalpostPos(), robot->position(),
                             field.friendlyGoalpostNeg())
                .abs();
        net_percent_open = best_shot_opt->second.toDegrees() / goal_angle.toDegrees();
    }

    // If we have a shot with a sufficiently large enough opening, and the deflection
    // angle that is reasonable, we should one-touch kick the ball towards the enemy net
    if (best_shot_opt && net_percent_open > MIN_SHOT_NET_PERCENT_OPEN &&
        abs_angle_between_pass_and_shot_vectors < MAX_DEFLECTION_FOR_ONE_TOUCH_SHOT)
    {
        return best_shot_opt;
    }
    return std::nullopt;
}

std::pair<Point, Angle> ReceiverTactic::getOneTimeShotPositionAndOrientation(
    const Robot& robot, const Ball& ball, const Point& best_shot_target)
{
    double dist_to_ball_in_dribbler =
        DIST_TO_FRONT_OF_ROBOT_METERS + BALL_MAX_RADIUS_METERS;
    Point ball_contact_point =
        robot.position() +
        Vector::createFromAngle(robot.orientation()).norm(dist_to_ball_in_dribbler);

    // Find the closest point to the ball contact point on the ball's trajectory
    Point closest_ball_pos = closestPointOnLine(ball_contact_point, ball.position(),
                                                ball.position() + ball.velocity());
    Ray shot(closest_ball_pos, best_shot_target - closest_ball_pos);

    Angle ideal_orientation      = getOneTimeShotDirection(shot, ball);
    Vector ideal_orientation_vec = Vector::createFromAngle(ideal_orientation);

    // The best position is determined such that the robot stays in the ideal
    // orientation, but moves the shortest distance possible to put its contact point
    // in the ball's path.
    Point ideal_position =
        closest_ball_pos - ideal_orientation_vec.norm(dist_to_ball_in_dribbler);

    return std::make_pair(ideal_position, ideal_orientation);
}
