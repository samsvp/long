```elixir
import actor("actor")

act Stack(init) do
    # helper to pop data into the stack, namespaced by `Stack`
    fun pop(stack) do
        head = actor::get(stack, :head)
        # returns the stack, meaning you can chain updates
        actor::update(stack, :pop)
        head
    end

    # helper to push data into the stack
    fun push(stack, value)
        actor::update(stack, { op: :append, value})

    fun peek(stack)
        actor::get(stack, :head)

    # must implement to update the inner state
    fun update(state, msg)
    | (state, { op: :append, value }) do
        [ value, ..state ]
    | (state, :pop) do
        match state
        | [head, ..tail] do tail
        | [] do []
        end
    end

    # must implement to get the inner state
    fun get(state, msg)
    | ([head, ..tail], :head) do head
    | ([], :head) do nil
    | ([head, ..tail], :tail) do tail
    | ([], :tail) do nil
    end

end

stack =
    Stack([])
    |> Stack::push(1)
    |> Stack::push(2)
    |> Stack::push(3)

Stack::peek(stack) |> println() # 3
Stack::pop(stack) |> println() # 3
Stack::peek(stack) |> println() # 2
```
