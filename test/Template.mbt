<h1>Blog Posts</h1>

@while true
    I'm running forever!
@endwhile

@for post in posts
    <h2>{{post.title}}</h2>
    <p>{{post.body}}</p>
@endfor

@if count(posts) == 1
    There is one post!
@else if count(posts) > 1
    There are multiple posts!
@else
    There are no posts!
@endif

@switch i
    @case 10
        i is equal to 10
    @case _
        i is not 10
@endswitch
