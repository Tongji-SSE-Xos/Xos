#include "hyc.h"

// 初始化链表
void init_list(list_t *lst)
{
    lst->head.prev = NULL;
    lst->tail.next = NULL;
    lst->head.next = &lst->tail;
    lst->tail.prev = &lst->head;
}

// 在 reference 结点前插入新结点 node
void insert_node_before(list_node_t *reference, list_node_t *node)
{
    node->prev = reference->prev;
    node->next = reference;

    reference->prev->next = node;
    reference->prev = node;
}

// 在 reference 结点后插入新结点 node
void insert_node_after(list_node_t *reference, list_node_t *node)
{
    node->prev = reference;
    node->next = reference->next;

    reference->next->prev = node;
    reference->next = node;
}

// 插入到链表头部
void list_push_front(list_t *lst, list_node_t *node)
{
    assert(!is_node_in_list(lst, node));
    insert_node_after(&lst->head, node);
}

// 移除链表头部后的第一个结点
list_node_t *list_pop_front(list_t *lst)
{
    assert(!is_list_empty(lst));

    list_node_t *node = lst->head.next;
    remove_node(node);

    return node;
}

// 插入到链表尾部前
void list_push_back(list_t *lst, list_node_t *node)
{
    assert(!is_node_in_list(lst, node));
    insert_node_before(&lst->tail, node);
}

// 移除链表尾部前的结点
list_node_t *list_pop_back(list_t *lst)
{
    assert(!is_list_empty(lst));

    list_node_t *node = lst->tail.prev;
    remove_node(node);

    return node;
}

// 查找链表中是否存在指定结点
bool is_node_in_list(list_t *lst, list_node_t *node)
{
    list_node_t *current = lst->head.next;
    while (current != &lst->tail)
    {
        if (current == node)
            return true;
        current = current->next;
    }
    return false;
}

// 从链表中删除指定结点
void remove_node(list_node_t *node)
{
    assert(node->prev != NULL);
    assert(node->next != NULL);

    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = NULL;
    node->prev = NULL;
}

// 判断链表是否为空
bool is_list_empty(list_t *lst)
{
    return (lst->head.next == &lst->tail);
}

// 获取链表的长度
u32 get_list_size(list_t *lst)
{
    list_node_t *current = lst->head.next;
    u32 size = 0;
    while (current != &lst->tail)
    {
        size++;
        current = current->next;
    }
    return size;
}

// 链表插入排序
void list_sorted_insert(list_t *lst, list_node_t *node, int offset)
{
    // 找到第一个比节点 key 大的节点，在其前插入新节点
    list_node_t *reference = &lst->tail;
    int node_key = element_node_key(node, offset);
    for (list_node_t *current = lst->head.next; current != &lst->tail; current = current->next)
    {
        int current_key = element_node_key(current, offset);
        if (current_key > node_key)
        {
            reference = current;
            break;
        }
    }

    assert(node->next == NULL);
    assert(node->prev == NULL);

    // 插入到找到的参考节点前
    insert_node_before(reference, node);
}
