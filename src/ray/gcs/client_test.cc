#include "gtest/gtest.h"

// TODO(pcm): get rid of this and replace with the type safe plasma event loop
extern "C" {
#include "hiredis/async.h"
#include "hiredis/hiredis.h"
#include "hiredis/adapters/ae.h"
}

#include "ray/gcs/client.h"
#include "ray/gcs/tables.h"

namespace ray {

aeEventLoop *loop;

class TestGcs : public ::testing::Test {
 public:
  TestGcs() {
    RAY_CHECK_OK(client_.Connect("127.0.0.1", 6379));
    job_id_ = UniqueID::from_random();
  }

 protected:
  gcs::AsyncGcsClient client_;
  UniqueID job_id_;
};

void ObjectAdded(gcs::AsyncGcsClient *client,
                 const UniqueID &id,
                 std::shared_ptr<ObjectTableDataT> data) {
  ASSERT_EQ(data->managers, std::vector<std::string>({"A", "B"}));
}

void Lookup(gcs::AsyncGcsClient *client,
            const UniqueID &id,
            std::shared_ptr<ObjectTableDataT> data) {
  ASSERT_EQ(data->managers, std::vector<std::string>({"A", "B"}));
  aeStop(loop);
}

TEST_F(TestGcs, TestObjectTable) {
  loop = aeCreateEventLoop(1024);
  RAY_CHECK_OK(client_.context()->AttachToEventLoop(loop));
  auto data = std::make_shared<ObjectTableDataT>();
  data->managers.push_back("A");
  data->managers.push_back("B");
  ObjectID object_id = ObjectID::from_random();
  RAY_CHECK_OK(
      client_.object_table().Add(job_id_, object_id, data, &ObjectAdded));
  RAY_CHECK_OK(
      client_.object_table().Lookup(job_id_, object_id, &Lookup, &Lookup));
  aeMain(loop);
  aeDeleteEventLoop(loop);
}

void TaskAdded(gcs::AsyncGcsClient *client,
               const TaskID &id,
               std::shared_ptr<TaskTableDataT> data) {
  ASSERT_EQ(data->scheduling_state, SchedulingState_SCHEDULED);
}

void TaskLookup(gcs::AsyncGcsClient *client,
                const TaskID &id,
                std::shared_ptr<TaskTableDataT> data) {
  ASSERT_EQ(data->scheduling_state, SchedulingState_SCHEDULED);
  aeStop(loop);
}

TEST_F(TestGcs, TestTaskTable) {
  loop = aeCreateEventLoop(1024);
  RAY_CHECK_OK(client_.context()->AttachToEventLoop(loop));
  auto data = std::make_shared<TaskTableDataT>();
  data->scheduling_state = SchedulingState_SCHEDULED;
  TaskID task_id = TaskID::from_random();
  RAY_CHECK_OK(client_.task_table().Add(job_id_, task_id, data, &TaskAdded));
  RAY_CHECK_OK(
      client_.task_table().Lookup(job_id_, task_id, &TaskLookup, &TaskLookup));
  aeMain(loop);
  aeDeleteEventLoop(loop);
}

}  // namespace
