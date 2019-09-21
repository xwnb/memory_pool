#include <iostream>
#include <vector>

using namespace std;

typedef unsigned long int ulong;
typedef unsigned short int ushort;
typedef unsigned int uint;

#define MEMPOOL_ALIGNMENT 8  //对齐长度
//内存块，每个内存块管理一大块内存，包括许多分配单元
class MemoryBlock {
 public:
  MemoryBlock(int nUnitSize, int nUnitAmount);
  ~MemoryBlock(){};
  static void *operator new(size_t, int nUnitSize, int nUnitAmount);
  static void operator delete(void *, int nUnitSize, int nUnitAmount){};
  static void operator delete(void *pBlock);

  int nSize;           //该内存块的大小，以字节为单位
  int nFree;           //该内存块还有多少可分配的单元
  int nFirst;          //当前可用单元的序号，从0开始
  MemoryBlock *pNext;  //指向下一个内存块
  char aData[1];  //用于标记分配单元开始的位置，分配单元从aData的位置开始
};

MemoryBlock::MemoryBlock(int nUnitSize, int nUnitAmount)
    : nSize(nUnitAmount * nUnitSize),
      nFree(nUnitAmount - 1),  //构造的时，已将第一个单元分配，所以减一
      nFirst(1),               //同上
      pNext(nullptr) {
  //初始化数组链表，将每个分配单元的下一个分配单元的序号写在当前单元的前两个字节中
  char *pData = aData;
  //最后一个位置不用写入
  for (int i = 1; i < nUnitAmount; i++) {
    (*(ushort *)pData) = i;
    pData += nUnitSize;
  }
}

void *MemoryBlock::operator new(size_t, int nUnitSize, int nUnitAmount) {
  void *begin_addr =
      ::operator new(sizeof(MemoryBlock) + nUnitSize * nUnitAmount);
  cout << "memory block begin address: " << begin_addr << endl;
  cout << "memory block end address: "
       << begin_addr + sizeof(MemoryBlock) + nUnitSize * nUnitAmount << endl;
  return begin_addr;
}

void MemoryBlock::operator delete(void *pBlock) { ::operator delete(pBlock); }

class MemoryPool {
 public:
  MemoryPool(int _nUnitSize, int _nGrowSize = 1024, int _nInitSize = 256);
  ~MemoryPool();
  void *Alloc();
  void Free(void *pFree);

 private:
  int nInitSize;        //初始大小
  int nGrowSize;        //增长大小
  int nUnitSize;        //分配单元大小
  MemoryBlock *pBlock;  //内存块链表
};

MemoryPool::MemoryPool(int _nUnitSize, int _nGrowSize /*= 1024*/,
                       int _nInitSize /*= 256*/) {
  nInitSize = _nInitSize;
  nGrowSize = _nGrowSize;
  pBlock = nullptr;
  if (_nUnitSize > 4)
    nUnitSize =
        (_nUnitSize + (MEMPOOL_ALIGNMENT - 1)) & ~(MEMPOOL_ALIGNMENT - 1);
  else if (_nUnitSize < 2)
    nUnitSize = 2;
  else
    nUnitSize = 4;
}

MemoryPool::~MemoryPool() {
  MemoryBlock *pMyBlock = pBlock;
  while (pMyBlock != nullptr) {
    pMyBlock = pMyBlock->pNext;
    delete (pMyBlock);
  }
}

void *MemoryPool::Alloc() {
  if (nullptr == pBlock) {
    cout << "alloc memory block\n";
    //首次生成MemoryBlock,new带参数，new了一个MemoryBlock类
    pBlock = (MemoryBlock *)new (nUnitSize, nInitSize)
        MemoryBlock(nUnitSize, nInitSize);
    cout << "alloc node address: " << (void *)pBlock->aData << endl;
    return (void *)pBlock->aData;
  }

  //找到符合条件的内存块
  MemoryBlock *pMyBlock = pBlock;
  cout << "init block address: " << pMyBlock << endl;
  while (pMyBlock != nullptr && 0 == pMyBlock->nFree) {
    pMyBlock = pMyBlock->pNext;
    cout << "switch block address: " << pMyBlock << endl;
  }

  cout << "current block address: " << pMyBlock << endl;

  if (pMyBlock != nullptr) {
    //找到了，进行分配
    char *pFree = pMyBlock->aData + pMyBlock->nFirst * nUnitSize;
    pMyBlock->nFirst = *((ushort *)pFree);
    pMyBlock->nFree--;
    cout << "alloc node address: " << (void *)pFree << endl;
    return (void *)pFree;
  } else {
    //没有找到，说明原来的内存块都满了，要再次分配

    if (0 == nGrowSize) {
      return nullptr;
    }

    cout << "alloc new memory block\n";
    pMyBlock = (MemoryBlock *)new (nUnitSize, nGrowSize)
        MemoryBlock(nUnitSize, nGrowSize);

    if (nullptr == pMyBlock) {
      return nullptr;
    }

    //进行一次插入操作
    pMyBlock->pNext = pBlock;
    pBlock = pMyBlock;

    cout << "alloc node address: " << (void *)pMyBlock->aData << endl;
    return (void *)pMyBlock->aData;
  }
}

void MemoryPool::Free(void *pFree) {
  cout << "to free node address: " << pFree << endl;
  //找到p所在的内存块
  MemoryBlock *pMyBlock = pBlock;
  MemoryBlock *pPreBlock = pBlock;
  while (((ulong)pMyBlock->aData > (ulong)pFree ||
          (ulong)pFree >= ((ulong)pMyBlock->aData + pMyBlock->nSize))) {
    cout << "nodes address range: " << (void *)pMyBlock->aData << "-"
         << (void *)pMyBlock->aData + pMyBlock->nSize << endl;
    pPreBlock = pMyBlock;
    pMyBlock = pMyBlock->pNext;
  }

  cout << "find block head: " << pMyBlock << endl;
  cout << "find nodes address range: " << (void *)pMyBlock->aData << "-"
       << (void *)pMyBlock->aData + pMyBlock->nSize << endl;

  if (nullptr != pMyBlock)  //该内存在本内存池中pMyBlock所指向的内存块中
  {
    // Step1 修改数组链表
    pMyBlock->nFree++;
    *((ushort *)pFree) = pMyBlock->nFirst;
    pMyBlock->nFirst =
        (ushort)((ulong)pFree - (ulong)pMyBlock->aData) / nUnitSize;

    // Step2 判断是否需要向OS释放内存
    if (pMyBlock->nSize == pMyBlock->nFree * nUnitSize) {
      if (pPreBlock == pBlock) {
        pPreBlock = pMyBlock->pNext;
      } else {
        pPreBlock->pNext = pMyBlock->pNext;
      }
      //在链表中删除该block
      cout << "delete memory block\n";
      delete (pMyBlock);
    }
    // } else {
    //   //将该block插入到队首
    //   if (pMyBlock != pBlock) {
    //     pPreBlock->pNext = pMyBlock->pNext;
    //     pMyBlock->pNext = pBlock;
    //     pBlock = pMyBlock;
    //     cout << "put forward block: " << pBlock << endl;
    //   }
    // }
  }
}

typedef struct MyStruct {
  int i;
  char ch;
} MyStruct;

int main(int argc, char const *argv[]) {
  cout << sizeof(MyStruct) << endl;
  MemoryPool memory_pool(sizeof(MyStruct), 3, 3);
  vector<MyStruct *> my_structs;
  for (int i = 0; i < 9; ++i) {
    MyStruct *my_struct = (MyStruct *)memory_pool.Alloc();
    my_struct->i = i * 10;
    my_structs.push_back(my_struct);
    cout << "num: " << i << " push_back\n";
  }

  memory_pool.Free((void *)my_structs[3]);
  memory_pool.Free((void *)my_structs[6]);
  memory_pool.Free((void *)my_structs[4]);
  memory_pool.Free((void *)my_structs[1]);
  memory_pool.Free((void *)my_structs[5]);
  memory_pool.Free((void *)my_structs[0]);
  memory_pool.Free((void *)my_structs[2]);
  memory_pool.Free((void *)my_structs[7]);
  memory_pool.Free((void *)my_structs[8]);
  // for (auto &stu : my_structs) {
  //   cout << "to free node " << stu->i << " address: " << stu << endl;
  //   memory_pool.Free((void *)stu);
  // }
  wait();
  return 0;
}
