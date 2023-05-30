<h1>Heap</h1>

<p>
This memory-manager is based on my <a href="http://www.github.com/svenbieg/clusters">Clusters</a> sorting-algorithm.<br />
Free space is mapped by size and by offset, so the smallest free block top most of the heap is returned.<br />
</p><br />

<img src="https://user-images.githubusercontent.com/12587394/103431851-2114df80-4bd7-11eb-82fd-5c87cd22f8e0.jpg" /><br />
<br />

<p>
Some parts are still missing. I'm going to make the map re-entrant, so the heap won't grow that fast anymore.<br />
There seems to be no problem, freeing pages can be cached in the free pages itself. And allocating can be made passive, not moving anything in the tree and setting zeros. The initial thread then cleans up the structure while coming down from the lowest level.<br />
<br />
Best regards,<br />
<br />
Sven Bieg
</p><br />

<br /><br /><br /><br /><br />
