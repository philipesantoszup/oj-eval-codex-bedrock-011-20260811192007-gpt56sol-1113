#ifndef SJTU_PRIORITY_QUEUE_HPP
#define SJTU_PRIORITY_QUEUE_HPP

#include <cstddef>
#include <functional>
#include "exceptions.hpp"

namespace sjtu {
/**
 * @brief a container like std::priority_queue which is a heap internal.
 * **Exception Safety**: The `Compare` operation might throw exceptions for certain data.
 * In such cases, any ongoing operation should be terminated, and the priority queue should be restored to its original state before the operation began.
 */
template<typename T, class Compare = std::less<T> >
class priority_queue {
private:
	struct node {
		T value;
		node *left;
		node *right;
		size_t null_path_length;

		explicit node(const T &value_)
			: value(value_), left(NULL), right(NULL), null_path_length(1) {}
	};

	struct copy_task {
		const node *source;
		node **destination;
		copy_task *next;

		copy_task(const node *source_, node **destination_, copy_task *next_)
			: source(source_), destination(destination_), next(next_) {}
	};

	node *root;
	size_t element_count;
	Compare compare;

	static size_t rank(node *current) {
		return current == NULL ? 0 : current->null_path_length;
	}

	bool lower_priority(const T &lhs, const T &rhs) {
		try {
			return compare(lhs, rhs);
		} catch (...) {
			throw runtime_error();
		}
	}

	/**
	 * Melds two leftist heaps.  Every recursive frame saves the fields it may
	 * modify, so an exception from Compare unwinds all changes made so far.
	 */
	node *meld(node *first, node *second) {
		if (first == NULL) return second;
		if (second == NULL) return first;
		if (lower_priority(first->value, second->value)) {
			node *temporary = first;
			first = second;
			second = temporary;
		}

		node *old_left = first->left;
		node *old_right = first->right;
		size_t old_rank = first->null_path_length;
		try {
			first->right = meld(first->right, second);
			if (rank(first->left) < rank(first->right)) {
				node *temporary = first->left;
				first->left = first->right;
				first->right = temporary;
			}
			first->null_path_length = rank(first->right) + 1;
		} catch (...) {
			first->left = old_left;
			first->right = old_right;
			first->null_path_length = old_rank;
			throw;
		}
		return first;
	}

	/** Delete a tree without recursion, using rotations to expose each node. */
	static void clear(node *current) {
		while (current != NULL) {
			if (current->left != NULL) {
				node *next = current->left;
				current->left = next->right;
				next->right = current;
				current = next;
			} else {
				node *next = current->right;
				delete current;
				current = next;
			}
		}
	}

	static void clear_tasks(copy_task *task) {
		while (task != NULL) {
			copy_task *next = task->next;
			delete task;
			task = next;
		}
	}

	/** Deep-copy iteratively, since a leftist heap may have a linear left side. */
	static node *clone(const node *source) {
		if (source == NULL) return NULL;
		node *result = NULL;
		copy_task *tasks = new copy_task(source, &result, NULL);
		try {
			while (tasks != NULL) {
				copy_task *task = tasks;
				tasks = tasks->next;
				const node *old_node = task->source;
				node **destination = task->destination;
				delete task;

				node *new_node = new node(old_node->value);
				new_node->null_path_length = old_node->null_path_length;
				*destination = new_node;
				if (old_node->right != NULL) {
					tasks = new copy_task(old_node->right, &new_node->right, tasks);
				}
				if (old_node->left != NULL) {
					tasks = new copy_task(old_node->left, &new_node->left, tasks);
				}
			}
		} catch (...) {
			clear_tasks(tasks);
			clear(result);
			throw;
		}
		return result;
	}

public:
	/**
	 * @brief default constructor
	 */
	priority_queue() : root(NULL), element_count(0), compare() {}

	/**
	 * @brief copy constructor
	 * @param other the priority_queue to be copied
	 */
	priority_queue(const priority_queue &other)
		: root(NULL), element_count(other.element_count), compare(other.compare) {
		root = clone(other.root);
	}

	/**
	 * @brief deconstructor
	 */
	~priority_queue() {
		clear(root);
	}

	/**
	 * @brief Assignment operator
	 * @param other the priority_queue to be assigned from
	 * @return a reference to this priority_queue after assignment
	 */
	priority_queue &operator=(const priority_queue &other) {
		if (this == &other) return *this;
		node *new_root = clone(other.root);
		try {
			compare = other.compare;
		} catch (...) {
			clear(new_root);
			throw;
		}
		clear(root);
		root = new_root;
		element_count = other.element_count;
		return *this;
	}

	/**
	 * @brief get the top element of the priority queue.
	 * @return a reference of the top element.
	 * @throws container_is_empty if empty() returns true
	 */
	const T &top() const {
		if (root == NULL) throw container_is_empty();
		return root->value;
	}

	/**
	 * @brief push new element to the priority queue.
	 * @param e the element to be pushed
	 */
	void push(const T &e) {
		node *added = new node(e);
		try {
			root = meld(root, added);
		} catch (...) {
			delete added;
			throw;
		}
		++element_count;
	}

	/**
	 * @brief delete the top element from the priority queue.
	 * @throws container_is_empty if empty() returns true
	 */
	void pop() {
		if (root == NULL) throw container_is_empty();
		node *removed = root;
		node *new_root = meld(root->left, root->right);
		root = new_root;
		delete removed;
		--element_count;
	}

	/**
	 * @brief return the number of elements in the priority queue.
	 * @return the number of elements.
	 */
	size_t size() const {
		return element_count;
	}

	/**
	 * @brief check if the container is empty.
	 * @return true if it is empty, false otherwise.
	 */
	bool empty() const {
		return element_count == 0;
	}

	/**
	 * @brief merge another priority_queue into this one.
	 * The other priority_queue will be cleared after merging.
	 * The complexity is at most O(logn).
	 * @param other the priority_queue to be merged.
	 */
	void merge(priority_queue &other) {
		if (this == &other || other.root == NULL) return;
		root = meld(root, other.root);
		element_count += other.element_count;
		other.root = NULL;
		other.element_count = 0;
	}
};

}

#endif
