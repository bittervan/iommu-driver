// #include <linux/slab.h>
#include "avl.h"

struct iova_node {
    uint8_t cnt;
    int height;
    uint64_t iova;
    struct iova_node *left;
    struct iova_node *right;
};

int iova_node_height(struct iova_node *node) {
    if (node == NULL)
        return 0;
    return node->height;
}

int iova_node_max(int a, int b) {
    return (a > b) ? a : b;
}

struct iova_node* iova_node_create(uint64_t iova) {
    struct iova_node* newNode = kmalloc(sizeof(struct iova_node), GFP_KERNEL);
    if (!newNode)
        return NULL;

    newNode->cnt = 1;
    newNode->height = 1;
    newNode->iova = iova;
    newNode->left = NULL;
    newNode->right = NULL;
    return newNode;
}

struct iova_node* iova_node_right_rotate(struct iova_node* y) {
    struct iova_node* x = y->left;
    struct iova_node* T2 = x->right;

    x->right = y;
    y->left = T2;

    y->height = iova_node_max(iova_node_height(y->left), iova_node_height(y->right)) + 1;
    x->height = iova_node_max(iova_node_height(x->left), iova_node_height(x->right)) + 1;

    return x;
}

struct iova_node* iova_node_left_rotate(struct iova_node* x) {
    struct iova_node* y = x->right;
    struct iova_node* T2 = y->left;

    y->left = x;
    x->right = T2;

    x->height = iova_node_max(iova_node_height(x->left), iova_node_height(x->right)) + 1;
    y->height = iova_node_max(iova_node_height(y->left), iova_node_height(y->right)) + 1;

    return y;
}

int iova_node_get_balance(struct iova_node* node) {
    if (node == NULL)
        return 0;
    return iova_node_height(node->left) - iova_node_height(node->right);
}

struct iova_node* iova_node_min_value_node(struct iova_node* node) {
    struct iova_node* current_node = node;

    while (current_node->left != NULL)
        current_node = current_node->left;

    return current_node;
}

struct iova_node* iova_node_insert(struct iova_node* root, uint64_t iova) {
    int balance = 0;
    if (root == NULL)
        return iova_node_create(iova);

    if (iova < root->iova)
        root->left = iova_node_insert(root->left, iova);
    else if (iova > root->iova)
        root->right = iova_node_insert(root->right, iova);
    else {
        root->cnt++;
        return root;
    }

    root->height = 1 + iova_node_max(iova_node_height(root->left), iova_node_height(root->right));

    balance = iova_node_get_balance(root);

    if (balance > 1 && iova < root->left->iova)
        return iova_node_right_rotate(root);

    if (balance < -1 && iova > root->right->iova)
        return iova_node_left_rotate(root);

    if (balance > 1 && iova > root->left->iova) {
        root->left = iova_node_left_rotate(root->left);
        return iova_node_right_rotate(root);
    }

    if (balance < -1 && iova < root->right->iova) {
        root->right = iova_node_right_rotate(root->right);
        return iova_node_left_rotate(root);
    }

    return root;
}

struct iova_node* iova_node_delete(struct iova_node* root, uint64_t iova) {
    int balance = 0;
    if (root == NULL)
        return root;

    if (iova < root->iova)
        root->left = iova_node_delete(root->left, iova);
    else if (iova > root->iova)
        root->right = iova_node_delete(root->right, iova);
    else {
        if (root->cnt > 1) {
            root->cnt--;
            return root;
        }

        if ((root->left == NULL) || (root->right == NULL)) {
            struct iova_node* temp = root->left ? root->left : root->right;

            if (temp == NULL) {
                temp = root;
                root = NULL;
            } else
                *root = *temp;

            kfree(temp);
        } else {
            struct iova_node* temp = iova_node_min_value_node(root->right);

            root->iova = temp->iova;
            root->right = iova_node_delete(root->right, temp->iova);
        }
    }

    if (root == NULL)
        return root;

    root->height = 1 + iova_node_max(iova_node_height(root->left), iova_node_height(root->right));

    balance = iova_node_get_balance(root);

    if (balance > 1 && iova_node_get_balance(root->left) >= 0)
        return iova_node_right_rotate(root);

    if (balance > 1 && iova_node_get_balance(root->left) < 0) {
        root->left = iova_node_left_rotate(root->left);
        return iova_node_right_rotate(root);
    }

    if (balance < -1 && iova_node_get_balance(root->right) <= 0)
        return iova_node_left_rotate(root);

    if (balance < -1 && iova_node_get_balance(root->right) > 0) {
        root->right = iova_node_right_rotate(root->right);
        return iova_node_left_rotate(root);
    }

    return root;
}

void iova_node_inorder_traversal(struct iova_node* root) {
    if (root != NULL) {
        iova_node_inorder_traversal(root->left);
        pr_info("(%llu, %d) ", root->iova, root->cnt);
        iova_node_inorder_traversal(root->right);
    }
}

int iova_node_get_count(struct iova_node* root, uint64_t iova) {
    if (root == NULL)
        return 0;

    if (iova < root->iova)
        return iova_node_get_count(root->left, iova);
    else if (iova > root->iova)
        return iova_node_get_count(root->right, iova);
    else
        return root->cnt;
}
